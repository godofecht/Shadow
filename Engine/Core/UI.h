// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include "Engine/Core/Geometry.h"
#include <SDL2/SDL.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

class Renderer;

void drawSimpleText(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color color, float scale = 1.0f);

// On-screen technical explanation overlay
class ExplanationOverlay {
public:
    ExplanationOverlay(int x, int y, int w);
    void addLine(const std::string& line);
    void clear();
    void render(Renderer* renderer);
private:
    int posX, posY, width;
    std::vector<std::string> lines;
};

// Interactive button for WASM and 2D games
class Button {
public:
    Button(int x, int y, int w, int h, const std::string& text);
    void update(int mx, int my, bool pressed);
    void render(Renderer* renderer);
    bool isClicked() const { return clicked; }
    
    // Legacy support for other examples
    void setLabel(const std::string& l) { text = l; }
    void setPosition(int _x, int _y) { x = _x; y = _y; }
    void setSize(int _w, int _h) { w = _w; h = _h; }
    void update(void* dummy) { (void)dummy; } // Placeholder for older API

private:
    int x, y, w, h;
    std::string text;
    bool hovered = false;
    bool clicked = false;
};

// Simple text display
class TextDisplay {
public:
    TextDisplay(int x, int y, const std::string& text = "");
    
    void setText(const std::string& text);
    const std::string& getText() const { return text; }
    
    void setPosition(int x, int y) { posX = x; posY = y; }
    void setColor(SDL_Color color);

    void render(Renderer* renderer);

    // Stream-like append
    TextDisplay& operator<<(const std::string& str);
    TextDisplay& operator<<(int value);

private:
    int posX, posY;
    std::string text;
    SDL_Color color = {255, 255, 255, 255};
};

// Game statistics display
class GameStats {
public:
    GameStats(int x, int y);
    
    void setStat(const std::string& key, int value);
    int getStat(const std::string& key) const;
    void addStat(const std::string& key, int delta);
    
    void render(Renderer* renderer);
    
private:
    int posX, posY;
    std::unordered_map<std::string, int> stats;
};
