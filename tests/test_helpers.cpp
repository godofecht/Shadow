// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for Helpers.h: PerlinNoiseGenerator + Easing
#include "../Engine/Core/Helpers.h"
#include "test_main.h"
#include <cmath>

REGISTER_TEST(test_HelpersPerlin_single_point)
{
    float noise = PerlinNoiseGenerator::perlinNoise(0.0f, 0.0f);
    ASSERT_NEAR(0.0f, noise, 0.01f);
}

REGISTER_TEST(test_HelpersPerlin_range)
{
    for (int x = 0; x < 10; ++x)
    {
        for (int y = 0; y < 10; ++y)
        {
            float noise = PerlinNoiseGenerator::perlinNoise(static_cast<float>(x) * 0.1f,
                                                            static_cast<float>(y) * 0.1f);
            ASSERT_TRUE(noise >= -1.0f && noise <= 1.0f);
        }
    }
}

REGISTER_TEST(test_HelpersPerlin_continuity)
{
    float noise1 = PerlinNoiseGenerator::perlinNoise(5.0f, 5.0f);
    float noise2 = PerlinNoiseGenerator::perlinNoise(5.1f, 5.0f);
    float diff = std::abs(noise1 - noise2);
    ASSERT_TRUE(diff < 0.5f);
}

REGISTER_TEST(test_HelpersPerlin_octaves)
{
    // Multi-octave noise stays bounded
    for (int i = 0; i < 20; ++i)
    {
        float n = PerlinNoiseGenerator::perlinNoise(i * 0.3f, i * 0.2f, 4);
        ASSERT_TRUE(n >= -2.0f && n <= 2.0f);
    }
}

REGISTER_TEST(test_HelpersPerlinMap_dimensions)
{
    auto map = PerlinNoiseGenerator::generatePerlinNoiseMap(10, 8);
    ASSERT_EQ(8, static_cast<int>(map.size()));
    for (const auto& row : map)
    {
        ASSERT_EQ(10, static_cast<int>(row.size()));
    }
}

REGISTER_TEST(test_HelpersPerlinMap_values_bounded)
{
    auto map = PerlinNoiseGenerator::generatePerlinNoiseMap(20, 20, 0.1f, 1);
    for (const auto& row : map)
    {
        for (float v : row)
        {
            ASSERT_TRUE(v >= -1.0f && v <= 1.0f);
        }
    }
}

REGISTER_TEST(test_Easing_Lerp)
{
    ASSERT_NEAR(5.0f, Easing::Lerp(0.0f, 10.0f, 0.5f), 0.001f);
    ASSERT_NEAR(0.0f, Easing::Lerp(0.0f, 10.0f, 0.0f), 0.001f);
    ASSERT_NEAR(10.0f, Easing::Lerp(0.0f, 10.0f, 1.0f), 0.001f);
    ASSERT_NEAR(7.5f, Easing::Lerp(5.0f, 10.0f, 0.5f), 0.001f);
}

REGISTER_TEST(test_Easing_quadratics)
{
    ASSERT_NEAR(0.0f, Easing::InQuad(0.0f), 0.001f);
    ASSERT_NEAR(0.25f, Easing::InQuad(0.5f), 0.001f);
    ASSERT_NEAR(1.0f, Easing::InQuad(1.0f), 0.001f);

    ASSERT_NEAR(0.0f, Easing::OutQuad(0.0f), 0.001f);
    ASSERT_NEAR(0.75f, Easing::OutQuad(0.5f), 0.001f);
    ASSERT_NEAR(1.0f, Easing::OutQuad(1.0f), 0.001f);

    ASSERT_NEAR(0.0f, Easing::InOutQuad(0.0f), 0.001f);
    ASSERT_NEAR(0.5f, Easing::InOutQuad(0.5f), 0.001f);
    ASSERT_NEAR(1.0f, Easing::InOutQuad(1.0f), 0.001f);
}

REGISTER_TEST(test_Easing_OutElastic)
{
    // OutElastic ends at 1
    ASSERT_NEAR(1.0f, Easing::OutElastic(1.0f), 0.001f);
    // Monotonic-ish at the tail
    float early = Easing::OutElastic(0.6f);
    float late = Easing::OutElastic(0.9f);
    ASSERT_TRUE(late <= 1.0f && early <= 1.0f);
}

REGISTER_TEST(test_Easing_OutBounce)
{
    ASSERT_NEAR(0.0f, Easing::OutBounce(0.0f), 0.001f);
    ASSERT_NEAR(1.0f, Easing::OutBounce(1.0f), 0.001f);
    // Bounce values stay in [0, 1]
    for (float t = 0.0f; t <= 1.0f; t += 0.05f)
    {
        float v = Easing::OutBounce(t);
        ASSERT_TRUE(v >= 0.0f && v <= 1.0f);
    }
}
