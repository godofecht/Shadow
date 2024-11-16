#pragma once
#include "Renderer.h"
#include "Helpers.h"
#include "Object.h"

class TileMap : public SimpleSprite
{
    std::vector<std::vector<float>> mapData;
    int tileWidth;
    int tileHeight;
    SDL_Color wallColor = {0, 0, 255, 255};  // Blue for walls
    SDL_Color emptyColor = {255, 255, 255, 255};  // White for empty spaces

public:

    // New constructor for (Renderer*, std::string)
    TileMap(Renderer* renderer, const std::string& mapIdentifier)
        : SimpleSprite(renderer, mapIdentifier)
    {
        // Initialize tileWidth, tileHeight, or load the map based on `mapIdentifier` as needed
        tileWidth = 32;  // Example default
        tileHeight = 32; // Example default

        // Load or generate the mapData based on mapIdentifier
        mapData = { {1, 1, 1, 1}, {1, 0, 0, 1}, {1, 0, 0, 1}, {1, 1, 1, 1} };

        isInitialized = true;
    }

    void setTileMapData(const std::vector<std::vector<float>>& mapData)
    {
        this->mapData = mapData;
    }

    void renderAndRunScripts(Renderer* renderer) override
    {
        if (!isActive || !isInitialized)
        {
            return;
        }

        tileWidth = static_cast<int>(getBounds().width / mapData[0].size());
        tileHeight = static_cast<int>(getBounds().height / mapData.size());

        SDL_Renderer* sdlRenderer = renderer->renderer;
        for (size_t i = 0; i < mapData.size(); ++i)
        {
            for (size_t j = 0; j < mapData[i].size(); ++j)
            {
                SDL_Rect tileRect = {
                    static_cast<int>(j * tileWidth),
                    static_cast<int>(i * tileHeight),
                    tileWidth,
                    tileHeight
                };

                // Set the color based on the tile type
                if (mapData[i][j] == 1)
                {
                    SDL_SetRenderDrawColor(sdlRenderer, wallColor.r, wallColor.g, wallColor.b, wallColor.a);
                }
                else
                {
                    SDL_SetRenderDrawColor(sdlRenderer, emptyColor.r, emptyColor.g, emptyColor.b, emptyColor.a);
                }



                // Draw the tile
                SDL_RenderFillRect(sdlRenderer, &tileRect);
                // Draw the border
                SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255); // Black color for the border
                SDL_RenderDrawRect(sdlRenderer, &tileRect);
            }
        }
    }
};