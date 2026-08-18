// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for PerlinNoiseGenerator (real implementation from Helpers.h)
#include "../Engine/Core/Helpers.h"
#include "test_main.h"
#include <cmath>

REGISTER_TEST(test_PerlinNoise_single_point)
{
    float noise = PerlinNoiseGenerator::perlinNoise(0.0f, 0.0f);
    // Perlin noise at integer coordinates should be 0
    ASSERT_NEAR(0.0f, noise, 0.01f);
}

REGISTER_TEST(test_PerlinNoise_range)
{
    for (int x = 0; x < 10; ++x)
    {
        for (int y = 0; y < 10; ++y)
        {
            float noise = PerlinNoiseGenerator::perlinNoise(static_cast<float>(x), static_cast<float>(y));
            ASSERT_TRUE(noise >= -1.0f && noise <= 1.0f);
        }
    }
}

REGISTER_TEST(test_PerlinNoise_continuity)
{
    float noise1 = PerlinNoiseGenerator::perlinNoise(5.0f, 5.0f);
    float noise2 = PerlinNoiseGenerator::perlinNoise(5.1f, 5.0f);

    float diff = std::abs(noise1 - noise2);
    ASSERT_TRUE(diff < 0.5f);
}

REGISTER_TEST(test_PerlinNoiseMap_dimensions)
{
    int width = 10;
    int height = 10;
    auto map = PerlinNoiseGenerator::generatePerlinNoiseMap(width, height);

    ASSERT_EQ(height, static_cast<int>(map.size()));
    for (const auto& row : map)
    {
        ASSERT_EQ(width, static_cast<int>(row.size()));
    }
}

REGISTER_TEST(test_PerlinNoiseMap_values)
{
    auto map = PerlinNoiseGenerator::generatePerlinNoiseMap(5, 5);

    for (const auto& row : map)
    {
        for (float value : row)
        {
            ASSERT_TRUE(value >= -1.0f && value <= 1.0f);
        }
    }
}

REGISTER_TEST(test_Interpolation)
{
    for (float t = 0.0f; t <= 1.0f; t += 0.1f)
    {
        float result = PerlinNoiseGenerator::perlinNoise(t * 100.0f, 0.0f);
        ASSERT_TRUE(result >= -1.0f && result <= 1.0f);
    }
}
