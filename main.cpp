// SPDX-License-Identifier: GPL-3.0-or-later OR ISC
// Shadow Engine - see LICENSE for details

#include "Engine/Core/SDLApp.h"
#include "Engine/Core/Helpers.h"
#include "Engine/Version.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <iostream>

#include <string>

class TopDownTileGame : public Game
{
public:
    TopDownTileGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {}

    void onStart() override {}

    void initializeComponents() override
    {
        auto scene = std::make_unique<Scene>();
        auto assets = getAssetManager();
        scene->setAssetManager(assets);
        scene->addItem(assets->createAsset<TileMap>("tilemap"));

        auto tilemap = getAssetManager()->getAsset<TileMap>("tilemap");

        // Generate Perlin noise map using Helpers.h
        std::vector<std::vector<float>> mapData = PerlinNoiseGenerator::generatePerlinNoiseMap(20, 20);

        // Set the float tile map data
        tilemap->setTileMapData(mapData);
        scene->getSpriteById("tilemap")->setBounds(0, 0, 700, 700);
        addScene(std::move(scene));
    }
};

int main(int argc, char* args[])
{
    (void)argc;
    (void)args;

    std::cout << "Shadow Engine v" << UMBRA_VERSION_STRING << std::endl;

    TopDownTileGame app("Shadow Engine v" UMBRA_VERSION_STRING, 700, 700);
    app.run();
    return 0;
}
