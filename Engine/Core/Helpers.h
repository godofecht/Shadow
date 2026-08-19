// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class PerlinNoiseGenerator
{
public:
    static float perlinNoise(float x, float y, int octaves = 1)
    {
        float frequency = 1.0f;
        float amplitude = 1.0f;
        float total = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            float scaledX = x * frequency;
            float scaledY = y * frequency;

            int x0 = static_cast<int>(floor(scaledX));
            int x1 = x0 + 1;
            int y0 = static_cast<int>(floor(scaledY));
            int y1 = y0 + 1;

            float xt = scaledX - static_cast<float>(x0);
            float yt = scaledY - static_cast<float>(y0);

            float n0 = dotGridPoint(x0, y0, scaledX, scaledY);
            float n1 = dotGridPoint(x1, y0, scaledX, scaledY);
            float xn = interpolate(n0, n1, xt);

            n0 = dotGridPoint(x0, y1, scaledX, scaledY);
            n1 = dotGridPoint(x1, y1, scaledX, scaledY);
            float yn = interpolate(n0, n1, xt);

            float interpolatedNoise = interpolate(xn, yn, yt);
            float mappedNoise = map(interpolatedNoise, -0.7f, 0.7f, -amplitude, amplitude);

            total += mappedNoise;

            frequency *= 2.0f;
            amplitude *= 0.5f;
        }

        return total;
    }

    static std::vector<std::vector<float>> generatePerlinNoiseMap(int width, int height, float scale = 0.1f, int octaves = 1)
    {
        std::vector<std::vector<float>> mapData;
        const std::size_t w = static_cast<std::size_t>(width);
        const std::size_t h = static_cast<std::size_t>(height);
        mapData.resize(h, std::vector<float>(w));
        for (std::size_t y = 0; y < h; ++y)
        {
            for (std::size_t x = 0; x < w; ++x)
            {
                mapData[y][x] = perlinNoise(static_cast<float>(x) * scale, static_cast<float>(y) * scale, octaves);
            }
        }
        return mapData;
    }

private:
    static float interpolate(float a, float b, float t)
    {
        return a + t * t * (3.0f - 2.0f * t) * (b - a);
    }

    static float map(float value, float fromLow, float fromHigh, float toLow, float toHigh)
    {
        value = std::min(std::max(value, fromLow), fromHigh);
        return toLow + (toHigh - toLow) * ((value - fromLow) / (fromHigh - fromLow));
    }

    static float dotGridPoint(int ix, int iy, float x, float y)
    {
        const unsigned w = 8 * sizeof(unsigned);
        const unsigned s = w / 2;
        unsigned a = static_cast<unsigned>(ix), b = static_cast<unsigned>(iy);
        a *= 3284157443u;
        b ^= a << s | a >> (w - s);
        b *= 1911520717u;
        a ^= b << s | b >> (w - s);
        a *= 2048419325u;

        float random = static_cast<float>(a) * (3.14159265f / static_cast<float>(~(~0u >> 1)));
        float dx = x - static_cast<float>(ix);
        float dy = y - static_cast<float>(iy);
        return (dx * cos(random) + dy * sin(random));
    }
};

class Easing {
public:
    static float Lerp(float a, float b, float t) { return a + (b - a) * t; }
    
    static float InQuad(float t) { return t * t; }
    static float OutQuad(float t) { return t * (2 - t); }
    static float InOutQuad(float t) { return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t; }
    
    static float OutElastic(float t) {
        float p = 0.3f;
        return static_cast<float>(pow(2, -10 * t) * sin((t - p / 4) * (2 * M_PI) / p) + 1);
    }

    static float OutBounce(float t) {
        if (t < (1 / 2.75f)) return 7.5625f * t * t;
        else if (t < (2 / 2.75f)) { t -= (1.5f / 2.75f); return 7.5625f * t * t + 0.75f; }
        else if (t < (2.5 / 2.75f)) { t -= (2.25f / 2.75f); return 7.5625f * t * t + 0.9375f; }
        else { t -= (2.625f / 2.75f); return 7.5625f * t * t + 0.984375f; }
    }
};
