// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for ParticleSystem (logic only, no rendering)
#include "../Engine/Rendering/ParticleSystem.h"
#include "test_main.h"
#include <cstdlib>

REGISTER_TEST(test_ParticleSystem_empty_initially)
{
    ParticleSystem ps(100);
    ASSERT_EQ(0, ps.size());
    ASSERT_TRUE(ps.empty());
}

REGISTER_TEST(test_ParticleSystem_emit)
{
    ParticleSystem ps(100);
    ps.emit(10.0f, 20.0f, 1.0f, 0.0f, {255, 0, 0, 255}, 1.0f, 4.0f);
    ASSERT_EQ(1, ps.size());
    ASSERT_FALSE(ps.empty());
}

REGISTER_TEST(test_ParticleSystem_capacity)
{
    ParticleSystem ps(10);
    for (int i = 0; i < 100; ++i)
    {
        ps.emit(0.0f, 0.0f, 0.0f, 0.0f, {255, 255, 255, 255}, 1.0f, 4.0f);
    }
    ASSERT_EQ(10, ps.size());  // Capped at maxParticles
}

REGISTER_TEST(test_ParticleSystem_update_moves_particles)
{
    // Deterministic-ish: emit a particle with a known velocity and verify
    // update() doesn't crash and keeps the particle alive initially.
    srand(42);
    ParticleSystem ps(100);
    ps.emit(0.0f, 0.0f, 1.0f, 1.0f, {255, 255, 255, 255}, 1.0f, 4.0f);
    ASSERT_EQ(1, ps.size());

    ps.update();
    ASSERT_EQ(1, ps.size());  // Still alive after one tick (life 1.0)
}

REGISTER_TEST(test_ParticleSystem_lifetime_decay)
{
    srand(7);
    ParticleSystem ps(100);
    ps.emit(0.0f, 0.0f, 0.0f, 0.0f, {255, 255, 255, 255}, 0.5f, 4.0f);

    // Life decays by ~0.01-0.03 per update, so 0.5 life dies within ~50 updates
    int ticks = 0;
    while (!ps.empty() && ticks < 200)
    {
        ps.update();
        ticks++;
    }
    ASSERT_TRUE(ps.empty());
    ASSERT_TRUE(ticks < 200);
}

REGISTER_TEST(test_ParticleSystem_emission_burst)
{
    srand(1);
    ParticleSystem ps(500);
    for (int i = 0; i < 50; ++i)
    {
        ps.emit(static_cast<float>(i), 0.0f, 0.0f, 0.0f, {0, 255, 0, 255}, 1.0f, 3.0f);
    }
    ASSERT_EQ(50, ps.size());

    // After enough updates all expire
    for (int i = 0; i < 200 && !ps.empty(); ++i)
    {
        ps.update();
    }
    ASSERT_TRUE(ps.empty());
}
