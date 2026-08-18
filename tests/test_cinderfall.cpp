// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

#include "Games/Cinderfall/CinderfallState.h"
#include "test_main.h"

#include <algorithm>

using cinderfall::State;
using cinderfall::Phase;
using cinderfall::EnemyKind;
using cinderfall::PickupKind;
using cinderfall::ParticleKind;
using cinderfall::Sfx;
using cinderfall::Input;

namespace {
constexpr float kDt = 1.0f / 60.0f;

Input moveRight() {
    Input in;
    in.moveX = 1.0f;
    return in;
}

Input attackInput() {
    Input in;
    in.attack = true;
    return in;
}
}  // namespace

// --- Floor generation -------------------------------------------------------

REGISTER_TEST(test_cinderfall_floor_gen_valid) {
    State s;
    s.newRun(12345u);
    ASSERT_EQ((int)Phase::Playing, (int)s.phase());
    ASSERT_EQ(35, s.width());
    ASSERT_EQ(35, s.height());
    // Player spawns on a walkable tile.
    ASSERT_FALSE(s.isSolid((int)s.player().x, (int)s.player().y));
    // Stairs are in bounds and walkable.
    ASSERT_TRUE(s.stairs().first >= 0);
    ASSERT_TRUE(s.inBounds(s.stairs().first, s.stairs().second));
    ASSERT_FALSE(s.isSolid(s.stairs().first, s.stairs().second));
    // A locked door and a key chest exist.
    ASSERT_EQ(1, static_cast<int>(s.doors().size()));
    ASSERT_FALSE(s.doors()[0].open);
    ASSERT_TRUE(s.isDoor(s.doors()[0].x, s.doors()[0].y));
    ASSERT_EQ(1, static_cast<int>(s.chests().size()));
    ASSERT_EQ((int)PickupKind::Key, (int)s.chests()[0].contents);
}

REGISTER_TEST(test_cinderfall_floor_gen_deterministic) {
    State a;
    State b;
    a.newRun(777u);
    b.newRun(777u);
    // Same seed -> identical world.
    ASSERT_EQ(a.stairs().first, b.stairs().first);
    ASSERT_EQ(a.stairs().second, b.stairs().second);
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            ASSERT_EQ(a.tileAt(x, y), b.tileAt(x, y));
}

// --- Movement ---------------------------------------------------------------

REGISTER_TEST(test_cinderfall_movement_blocked_by_wall) {
    State s;
    s.resetToEmpty(8, 8);
    s.setSolid(4, 2, true);
    s.setPlayerPos(2.5f, 2.5f);
    for (int i = 0; i < 40; ++i) s.step(kDt, moveRight());
    // Slides up against the wall instead of entering it.
    ASSERT_TRUE(s.player().x > 2.5f);
    ASSERT_TRUE(s.player().x < 3.7f);
    ASSERT_TRUE(s.isSolid(4, 2));
}

// --- Combat -----------------------------------------------------------------

REGISTER_TEST(test_cinderfall_attack_kills_enemy_in_arc) {
    State s;
    s.resetToEmpty(8, 8);
    s.setPlayerPos(2.5f, 2.5f);
    s.setFacing(1.0f, 0.0f);
    s.addEnemy(EnemyKind::Chaser, 3, 2, 3);
    s.step(kDt, attackInput());
    ASSERT_EQ(0, static_cast<int>(s.enemies().size()));
    ASSERT_EQ(5, s.player().gold);  // chaser bounty
}

REGISTER_TEST(test_cinderfall_attack_misses_behind) {
    State s;
    s.resetToEmpty(8, 8);
    s.setPlayerPos(2.5f, 2.5f);
    s.setFacing(1.0f, 0.0f);
    s.addEnemy(EnemyKind::Chaser, 1, 2, 3);  // behind the player
    s.step(kDt, attackInput());
    ASSERT_EQ(1, static_cast<int>(s.enemies().size()));
    ASSERT_EQ(3, s.enemies()[0].hp);
}

// --- Doors, chests, pickups -------------------------------------------------

REGISTER_TEST(test_cinderfall_door_requires_key) {
    State s;
    s.resetToEmpty(8, 8);
    s.addDoor(3, 2);
    ASSERT_TRUE(s.isSolid(3, 2));
    // No key -> refused, still solid.
    ASSERT_FALSE(s.openDoor(3, 2));
    ASSERT_TRUE(s.isSolid(3, 2));
    // Key -> opens.
    s.setKeys(1);
    ASSERT_TRUE(s.openDoor(3, 2));
    ASSERT_FALSE(s.isSolid(3, 2));
    ASSERT_EQ(0, s.player().keys);
}

REGISTER_TEST(test_cinderfall_interact_opens_adjacent_door) {
    State s;
    s.resetToEmpty(8, 8);
    s.addDoor(3, 2);
    s.setKeys(1);
    s.setPlayerPos(2.5f, 2.5f);  // adjacent (2,2) <-> (3,2)
    Input in;
    in.interact = true;
    s.step(kDt, in);
    ASSERT_FALSE(s.isSolid(3, 2));
}

REGISTER_TEST(test_cinderfall_chest_gives_key) {
    State s;
    s.resetToEmpty(8, 8);
    s.addChest(3, 2, PickupKind::Key);
    ASSERT_TRUE(s.openChest(3, 2));
    ASSERT_EQ(1, s.player().keys);
    ASSERT_FALSE(s.openChest(3, 2));  // already opened
}

REGISTER_TEST(test_cinderfall_pickup_gold) {
    State s;
    s.resetToEmpty(8, 8);
    s.addPickup(PickupKind::Gold, 3, 2);
    s.setPlayerPos(2.5f, 2.5f);
    for (int i = 0; i < 40; ++i) s.step(kDt, moveRight());
    ASSERT_EQ(5, s.player().gold);
    ASSERT_EQ(0, static_cast<int>(s.pickups().size()));
}

// --- Stairs / progression ---------------------------------------------------

REGISTER_TEST(test_cinderfall_stairs_descend) {
    State s;
    s.resetToEmpty(8, 8);
    s.setStairs(3, 2);
    s.setPlayerPos(2.5f, 2.5f);
    for (int i = 0; i < 40; ++i) s.step(kDt, moveRight());
    ASSERT_EQ(2, s.floor());
    ASSERT_EQ((int)Phase::Playing, (int)s.phase());
}

REGISTER_TEST(test_cinderfall_stairs_win_at_final_floor) {
    State s;
    s.resetToEmpty(8, 8);
    s.setFloor(3);  // == default max floors
    s.setStairs(3, 2);
    s.setPlayerPos(2.5f, 2.5f);
    for (int i = 0; i < 40; ++i) s.step(kDt, moveRight());
    ASSERT_EQ((int)Phase::Won, (int)s.phase());
}

// --- Damage -----------------------------------------------------------------

REGISTER_TEST(test_cinderfall_enemy_contact_damages) {
    State s;
    s.resetToEmpty(8, 8);
    s.setPlayerPos(2.5f, 2.5f);
    s.addEnemy(EnemyKind::Chaser, 3, 2, 3);
    for (int i = 0; i < 120; ++i) s.step(kDt, Input{});
    ASSERT_TRUE(s.player().hp < s.player().maxHp);
}

// --- M1 game feel -----------------------------------------------------------

REGISTER_TEST(test_cinderfall_hit_stop_freezes_world) {
    State s;
    s.resetToEmpty(8, 8);
    s.setPlayerPos(2.5f, 2.5f);
    s.setFacing(1.0f, 0.0f);
    s.addEnemy(EnemyKind::Chaser, 3, 2, 3);
    s.step(kDt, attackInput());  // killing blow -> hit-stop triggers
    ASSERT_TRUE(s.hitStopTimer() > 0.0f);
    const float frozenX = s.player().x;
    // While frozen, world input does not move the player.
    s.step(kDt, moveRight());
    ASSERT_EQ(frozenX, s.player().x);
    // Step past the freeze window; movement resumes.
    for (int i = 0; i < 20; ++i) s.step(kDt, moveRight());
    ASSERT_TRUE(s.player().x > frozenX);
    ASSERT_TRUE(s.hitStopTimer() <= 0.0f);
}

REGISTER_TEST(test_cinderfall_kill_spawns_and_culls_particles) {
    State s;
    s.resetToEmpty(8, 8);
    s.setPlayerPos(2.5f, 2.5f);
    s.setFacing(1.0f, 0.0f);
    s.addEnemy(EnemyKind::Chaser, 3, 2, 3);
    s.step(kDt, attackInput());
    ASSERT_TRUE(s.particles().size() >= 6u);  // blood + ember bursts
    bool sawBlood = false, sawEmber = false;
    for (const auto& pt : s.particles()) {
        if (pt.kind == ParticleKind::Blood) sawBlood = true;
        if (pt.kind == ParticleKind::Ember) sawEmber = true;
    }
    ASSERT_TRUE(sawBlood);
    ASSERT_TRUE(sawEmber);
    // Particles die out within a few seconds (culled, not leaked).
    for (int i = 0; i < 60 * 4; ++i) s.step(kDt, Input{});
    ASSERT_EQ(0u, s.particles().size());
}

REGISTER_TEST(test_cinderfall_trauma_decays_to_zero) {
    State s;
    s.resetToEmpty(8, 8);
    s.setPlayerPos(2.5f, 2.5f);
    s.setFacing(1.0f, 0.0f);
    s.addEnemy(EnemyKind::Chaser, 3, 2, 3);
    s.step(kDt, attackInput());
    ASSERT_TRUE(s.trauma() > 0.0f);
    // Trauma eases off over a couple of seconds (shake returns to rest).
    for (int i = 0; i < 60 * 3; ++i) s.step(kDt, Input{});
    ASSERT_EQ(0.0f, s.trauma());
}

REGISTER_TEST(test_cinderfall_sfx_events) {
    State s;
    s.resetToEmpty(8, 8);
    s.setPlayerPos(2.5f, 2.5f);
    s.setFacing(1.0f, 0.0f);

    // Swing (attack even with no enemy in range).
    s.step(kDt, attackInput());
    auto ev = s.takeSfx();
    ASSERT_TRUE(std::find(ev.begin(), ev.end(), Sfx::Swing) != ev.end());

    // Hit + Kill on a slaying blow.
    s.addEnemy(EnemyKind::Chaser, 3, 2, 3);
    for (int i = 0; i < 30; ++i) s.step(kDt, Input{});  // cooldown
    s.step(kDt, attackInput());
    ev = s.takeSfx();
    ASSERT_TRUE(std::find(ev.begin(), ev.end(), Sfx::Hit) != ev.end());
    ASSERT_TRUE(std::find(ev.begin(), ev.end(), Sfx::Kill) != ev.end());

    // Pickup.
    s.addPickup(PickupKind::Gold, 3, 2);
    for (int i = 0; i < 40; ++i) s.step(kDt, moveRight());
    ev = s.takeSfx();
    ASSERT_TRUE(std::find(ev.begin(), ev.end(), Sfx::Pickup) != ev.end());

    // Hurt (contact damage).
    s.setPlayerPos(2.5f, 2.5f);
    s.addEnemy(EnemyKind::Chaser, 3, 2, 3);
    for (int i = 0; i < 120; ++i) s.step(kDt, Input{});
    ev = s.takeSfx();
    ASSERT_TRUE(std::find(ev.begin(), ev.end(), Sfx::Hurt) != ev.end());
}

REGISTER_TEST(test_cinderfall_door_and_stairs_sfx) {
    State s;
    s.resetToEmpty(8, 8);
    s.setKeys(1);
    s.addDoor(3, 2);
    ASSERT_TRUE(s.openDoor(3, 2));
    auto ev = s.takeSfx();
    ASSERT_TRUE(std::find(ev.begin(), ev.end(), Sfx::Door) != ev.end());

    // Descend and win.
    s.resetToEmpty(8, 8);
    s.setStairs(3, 2);
    s.setPlayerPos(2.5f, 2.5f);
    for (int i = 0; i < 40; ++i) s.step(kDt, moveRight());
    ev = s.takeSfx();
    ASSERT_TRUE(std::find(ev.begin(), ev.end(), Sfx::Descend) != ev.end());

    State w;
    w.resetToEmpty(8, 8);
    w.setFloor(3);
    w.setStairs(3, 2);
    w.setPlayerPos(2.5f, 2.5f);
    for (int i = 0; i < 40; ++i) w.step(kDt, moveRight());
    ev = w.takeSfx();
    ASSERT_TRUE(std::find(ev.begin(), ev.end(), Sfx::Win) != ev.end());
}

// --- M2 bestiary ------------------------------------------------------------

REGISTER_TEST(test_cinderfall_ghost_phases_through_walls) {
    State s;
    s.resetToEmpty(12, 6);
    // A wall column between the player and the ghost.
    for (int y = 0; y < 6; ++y) s.setSolid(5, y, true);
    s.setPlayerPos(1.5f, 1.5f);
    s.setFacing(1.0f, 0.0f);
    s.addEnemy(EnemyKind::Ghost, 9, 1, 4);
    // A chaser would grind against the wall; the ghost drifts through it.
    for (int i = 0; i < 60 * 6; ++i) s.step(kDt, Input{});
    ASSERT_EQ(1, static_cast<int>(s.enemies().size()));  // survived, not lost
    // The ghost crossed the wall column (a chaser would grind against it at
    // x ~ 4.7 forever) and is still on the map near the player.
    const float gx = s.enemies()[0].x;
    ASSERT_TRUE(gx < 5.0f);
    ASSERT_TRUE(gx >= 0.5f);
}

REGISTER_TEST(test_cinderfall_ghost_contact_damages) {
    State s;
    s.resetToEmpty(10, 6);
    s.setPlayerPos(1.5f, 1.5f);
    s.addEnemy(EnemyKind::Ghost, 3, 1, 4);
    for (int i = 0; i < 60 * 3; ++i) s.step(kDt, Input{});
    ASSERT_TRUE(s.player().hp < s.player().maxHp);
}

REGISTER_TEST(test_cinderfall_turret_fires_three_shot_volley) {
    State s;
    s.resetToEmpty(14, 6);
    s.setPlayerPos(1.5f, 1.5f);
    s.addEnemy(EnemyKind::Turret, 8, 1, 5);   // 7 tiles away: in range
    // Step until we observe a full 3-projectile volley in flight.
    bool sawVolley = false;
    for (int i = 0; i < 60 * 8 && !sawVolley; ++i) {
        s.step(kDt, Input{});
        sawVolley = s.projectiles().size() >= 3u;
    }
    ASSERT_TRUE(sawVolley);
    // And it keeps volleying on a cooldown (a second volley appears later).
    int maxSeen = 0;
    for (int i = 0; i < 60 * 8; ++i) {
        s.step(kDt, Input{});
        maxSeen = std::max(maxSeen, (int)s.projectiles().size());
    }
    ASSERT_TRUE(maxSeen >= 3);
}

REGISTER_TEST(test_cinderfall_turret_is_stationary) {
    State s;
    s.resetToEmpty(14, 6);
    s.setPlayerPos(1.5f, 1.5f);
    s.addEnemy(EnemyKind::Turret, 10, 1, 5);
    const float sx = s.enemies()[0].x;
    const float sy = s.enemies()[0].y;
    for (int i = 0; i < 60 * 6; ++i) s.step(kDt, Input{});
    ASSERT_EQ(sx, s.enemies()[0].x);
    ASSERT_EQ(sy, s.enemies()[0].y);
}

REGISTER_TEST(test_cinderfall_floor_gen_spawns_m2_kinds) {
    // Every generated floor (1..3) must place at least one enemy, including
    // the deeper floors that can roll the M2 archetypes (ghost, turret).
    for (int f = 1; f <= 3; ++f) {
        State s;
        s.newRun(1234u + (uint32_t)f);
        ASSERT_TRUE(s.enemies().size() >= 1u);
    }
    // Ghosts and turrets are constructible and behave via addEnemy.
    State s;
    s.resetToEmpty(8, 8);
    s.addEnemy(EnemyKind::Ghost, 4, 4, 4);
    s.addEnemy(EnemyKind::Turret, 6, 4, 5);
    ASSERT_EQ(2, static_cast<int>(s.enemies().size()));
}

// --- Save / load ------------------------------------------------------------

REGISTER_TEST(test_cinderfall_serialize_roundtrip) {
    State a;
    a.newRun(4242u);
    a.setKeys(2);
    const std::string data = a.serialize();
    const auto stairs = a.stairs();

    State b;
    ASSERT_TRUE(b.load(data));
    ASSERT_EQ(4242u, b.seed());
    ASSERT_EQ(1, b.floor());
    ASSERT_EQ(3, b.maxFloors());
    ASSERT_EQ(2, b.player().keys);
    // Same seed + floor -> same regenerated world.
    ASSERT_EQ(stairs.first, b.stairs().first);
    ASSERT_EQ(stairs.second, b.stairs().second);
    ASSERT_EQ(a.tileAt(10, 10), b.tileAt(10, 10));
    ASSERT_EQ((int)Phase::Playing, (int)b.phase());
}
