// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// PerlinNoise - renders a procedurally generated Perlin-noise terrain as a
// TileMap. Demonstrates the engine's built-in PerlinNoiseGenerator
// (Engine/Core/Helpers.h) and the Scene / AssetManager / TileMap path.
//
// The generator emits [-amplitude, amplitude] values; the map is
// thresholded at 0 so it renders as solid land/water tiles (TileMap draws
// cells equal to 1 as walls and everything else as empty).

#include "Engine/Core/SDLApp.h"

#include <cmath>
#include <vector>

class TopDownTileGame : public Game
{
public:
    TopDownTileGame (const char* windowTitle, int width, int height)
        : Game(windowTitle, width, height)
    {
    }

    void initializeComponents() override
    {
        constexpr int kMapW = 20;
        constexpr int kMapH = 20;

        auto scene = std::make_unique<Scene>();
        auto assets = getAssetManager();
        scene->setAssetManager (assets);
        scene->addItem (assets->createAsset<TileMap> ("tilemap"));

        // The engine's generator (Helpers.h): two octaves of noise at a
        // fixed scale, thresholded to a binary land/water tile map.
        auto tilemap = assets->getAsset<TileMap> ("tilemap");
        std::vector<std::vector<float>> mapData =
            PerlinNoiseGenerator::generatePerlinNoiseMap (kMapW, kMapH, 0.12f, 2);
        for (auto& row : mapData)
        {
            for (auto& cell : row)
            {
                cell = (cell > 0.0f) ? 1.0f : 0.0f;
            }
        }
        tilemap->setTileMapData (mapData);
        scene->getSpriteById ("tilemap")->setBounds (0, 0, 700, 700);

        addScene (std::move (scene));
    }
};

int main ([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static TopDownTileGame app ("BrainRot Engine", 700, 700);
#else
    TopDownTileGame app ("BrainRot Engine", 700, 700);
#endif
    app.run();
    return 0;
}
