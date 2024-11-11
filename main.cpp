#include "SDLApp.h"

#include <vector>
#include <cmath>
#include <cstdlib>

#include <vector>
#include <cmath>
#include <algorithm>

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

    static std::vector<std::vector<float>> generatePerlinNoiseMap(int width, int height, float threshold = 0.5f)
    {
        std::vector<std::vector<float>> mapData;
        mapData.resize(height, std::vector<float>(width));
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                float noiseValue = perlinNoise(static_cast<float>(x), static_cast<float>(y));
                mapData[y][x] = noiseValue;//(noiseValue > threshold) ? 1 : 0; // Thresholding to get a binary map
            }
        }

        std::cout << "Perlin noise map generated." << std::endl;
        std::cout << "Map data: " << std::endl;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                std::cout << mapData[y][x] << " ";
            }
            std::cout << std::endl;
        }

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                std::cout << mapData[y][x] << " ";
            }
            std::cout << std::endl;
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
        Vector2 rand = getRandom(ix, iy);
        float dx = x - static_cast<float>(ix);
        float dy = y - static_cast<float>(iy);
        return (dx * rand.x + dy * rand.y);
    }

    struct Vector2
    {
        float x, y;
    };

    static Vector2 getRandom(int x, int y)
    {
        const unsigned w = 8 * sizeof(unsigned);
        const unsigned s = w / 2;
        unsigned a = x, b = y;
        a *= 3284157443u;
        b ^= a << s | a >> (w - s);
        b *= 1911520717u;
        a ^= b << s | b >> (w - s);
        a *= 2048419325u;

        float random = a * (3.14159265f / ~(~0u >> 1));
        Vector2 v;
        v.x = cos(random);
        v.y = sin(random);
        return v;
    }
};

inline std::vector<std::vector<float>> generatePerlinNoiseMap(int width, int height)
{
    std::vector<std::vector<float>> mapData(height, std::vector<float>(width));

    // Generate Perlin noise values directly
    mapData = PerlinNoiseGenerator::generatePerlinNoiseMap(width, height);

    return mapData;
}


class TopDownTileGame : public Game
{
    public:
    TopDownTileGame (const char* title, int width, int height) : Game (title, width, height)
    {

    }

    void onStart() override
    {

    }

    void initializeComponents() override
    {
        auto scene = std::make_unique<Scene>();
        auto assetManager = getAssetManager();
        scene->setAssetManager(assetManager);
        scene->addItem(assetManager->createAsset<TileMap>("tilemap"));

        auto& tilemap = getAssetManager()->getAsset<TileMap>("tilemap");

        // Generate float map data
        std::vector<std::vector<float>> mapData = generatePerlinNoiseMap(20, 20);

        // Set the float tile map data (assuming TileMap supports float data)
        tilemap->setTileMapData(mapData);
        scene->getSpriteById("tilemap")->setBounds(0, 0, 700, 700);
        addScene(std::move(scene));
    }
};

int main (int argc, char* args[])
{
    TopDownTileGame app ("BrainRot Engine", 700, 700);
    app.run();
    return 0;
}