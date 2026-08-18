#include "../Examples/Roguelike/RoguelikeState.h"
#include "test_main.h"

static void makeSimpleArena(RoguelikeState& s, int w = 8, int h = 8) {
    s.startRun(1, 3, 40, 10);
    s.startLevelMap(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool border = (x == 0 || y == 0 || x == w - 1 || y == h - 1);
            s.setSolid(x, y, border);
        }
    }
}

REGISTER_TEST(test_roguelike_blocked_by_wall) {
    RoguelikeState s;
    makeSimpleArena(s);
    s.setPlayerPos(1, 1);
    auto r = s.playerStep(-1, 0, 0);
    ASSERT_TRUE(r.blocked);
    ASSERT_EQ(s.getPlayer().x, 1);
    ASSERT_EQ(s.getPlayer().y, 1);
}

REGISTER_TEST(test_roguelike_combat_and_enemy_death) {
    RoguelikeState s;
    makeSimpleArena(s);
    s.setPlayerPos(2, 2);
    s.addEnemy(3, 2, 8, 4);

    auto r = s.playerStep(1, 0, 0); // dmg = 10 + (0 % 5) => 10
    ASSERT_TRUE(r.fought);
    ASSERT_EQ(r.damageToEnemy, 10);
    ASSERT_EQ(r.damageToPlayer, 0);
    ASSERT_FALSE(s.getEnemies()[0].alive);
    ASSERT_EQ(s.getPlayer().health, 40);
}

REGISTER_TEST(test_roguelike_combat_player_dies) {
    RoguelikeState s;
    s.startRun(1, 3, 5, 1);
    s.startLevelMap(8, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            s.setSolid(x, y, (x == 0 || y == 0 || x == 7 || y == 7));
        }
    }
    s.setPlayerPos(2, 2);
    s.addEnemy(3, 2, 50, 6);

    auto r = s.playerStep(1, 0, 0);
    ASSERT_TRUE(r.fought);
    ASSERT_TRUE(r.died);
    ASSERT_EQ((int)s.getPhase(), (int)RoguelikePhase::Dead);
    ASSERT_EQ(s.getPlayer().health, 0);
}

REGISTER_TEST(test_roguelike_gold_and_descend) {
    RoguelikeState s;
    makeSimpleArena(s);
    s.setPlayerPos(2, 2);
    s.addGold(3, 2);
    s.setStairs(4, 2);

    auto r1 = s.playerStep(1, 0, 7);
    ASSERT_TRUE(r1.moved);
    ASSERT_TRUE(r1.collectedGold);
    ASSERT_EQ(s.getGoldCount(), 10);

    auto r2 = s.playerStep(1, 0, 11);
    ASSERT_TRUE(r2.descended);
    ASSERT_EQ(s.getLevel(), 2);
    ASSERT_EQ((int)s.getPhase(), (int)RoguelikePhase::Playing);
}

REGISTER_TEST(test_roguelike_win_on_final_stairs) {
    RoguelikeState s;
    s.startRun(3, 3, 30, 8); // already at final level
    s.startLevelMap(8, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            s.setSolid(x, y, (x == 0 || y == 0 || x == 7 || y == 7));
        }
    }
    s.setPlayerPos(2, 2);
    s.setStairs(3, 2);

    auto r = s.playerStep(1, 0, 0);
    ASSERT_TRUE(r.won);
    ASSERT_EQ((int)s.getPhase(), (int)RoguelikePhase::Won);
}
