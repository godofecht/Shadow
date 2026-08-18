// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

#include "Engine/Core/UI.h"
#include "Engine/Rendering/Renderer.h"
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <sstream>

static void drawBlockText(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color color, float scale) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int curX = x;
    for (char c : text) {
        (void)c;
        int charW = (int)(8 * scale);
        int charH = (int)(12 * scale);
        SDL_Rect r = { curX, y, charW - 2, charH };
        SDL_RenderFillRect(renderer, &r);
        curX += charW;
    }
}

// Helper for text rendering in overlays/UI.
void drawSimpleText(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color color, float scale) {
    if (text.empty()) return;

    if (!TTF_WasInit() && TTF_Init() != 0) {
        drawBlockText(renderer, x, y, text, color, scale);
        return;
    }

    int fontSize = std::max(10, (int)(18.0f * scale));
    TTF_Font* font = TTF_OpenFont("default.ttf", fontSize);
    if (!font) {
        font = TTF_OpenFont("/default.ttf", fontSize);
    }
    if (!font) {
        drawBlockText(renderer, x, y, text, color, scale);
        return;
    }

    SDL_Surface* textSurface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!textSurface) {
        TTF_CloseFont(font);
        drawBlockText(renderer, x, y, text, color, scale);
        return;
    }

    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        TTF_CloseFont(font);
        drawBlockText(renderer, x, y, text, color, scale);
        return;
    }

    SDL_Rect dstRect = {x, y, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, nullptr, &dstRect);

    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
    TTF_CloseFont(font);
}

// ExplanationOverlay implementation
ExplanationOverlay::ExplanationOverlay(int x, int y, int w) : posX(x), posY(y), width(w) {}
void ExplanationOverlay::addLine(const std::string& line) { lines.push_back(line); }
void ExplanationOverlay::clear() { lines.clear(); }
void ExplanationOverlay::render(Renderer* renderer) {
    if (lines.empty()) return;
    
    // Draw background panel
    SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer->renderer, 0, 0, 0, 180);
    SDL_Rect bg = { posX - 10, posY - 10, width + 20, (int)(lines.size() * 18 + 20) };
    SDL_RenderFillRect(renderer->renderer, &bg);
    
    int curY = posY;
    for (const auto& line : lines) {
        drawSimpleText(renderer->renderer, posX, curY, line, {200, 200, 255, 255}, 0.7f);
        curY += 18;
    }
}

// Button implementation
Button::Button(int x, int y, int w, int h, const std::string& text) : x(x), y(y), w(w), h(h), text(text) {}
void Button::update(int mx, int my, bool pressed) {
    hovered = (mx >= x && mx <= x + w && my >= y && my <= y + h);
    clicked = hovered && pressed;
}
void Button::render(Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer->renderer, hovered ? 100 : 60, 60, 80, 255);
    SDL_Rect r = { x, y, w, h };
    SDL_RenderFillRect(renderer->renderer, &r);
    drawSimpleText(renderer->renderer, x + 10, y + h/2 - 6, text, {255, 255, 255, 255}, 0.7f);
}

// TextDisplay implementation
TextDisplay::TextDisplay(int x, int y, const std::string& text)
    : posX(x), posY(y), text(text) {
    color = {255, 255, 255, 255};
}

void TextDisplay::setText(const std::string& _text) {
    this->text = _text;
}

void TextDisplay::setColor(SDL_Color _color) {
    this->color = _color;
}

void TextDisplay::render(Renderer* renderer) {
    if (text.empty()) return;
    drawSimpleText(renderer->renderer, posX, posY, text, color);
}

TextDisplay& TextDisplay::operator<<(const std::string& str) {
    text += str;
    return *this;
}

TextDisplay& TextDisplay::operator<<(int val) {
    text += std::to_string(val);
    return *this;
}

// GameStats implementation
GameStats::GameStats(int x, int y) : posX(x), posY(y) {
}

void GameStats::setStat(const std::string& key, int value) {
    stats[key] = value;
}

int GameStats::getStat(const std::string& key) const {
    auto it = stats.find(key);
    if (it != stats.end()) return it->second;
    return 0;
}

void GameStats::addStat(const std::string& key, int delta) {
    stats[key] += delta;
}

void GameStats::render(Renderer* renderer) {
    int yOffset = 0;
    for (auto const& [key, val] : stats) {
        std::string statText = key + ": " + std::to_string(val);
        drawSimpleText(renderer->renderer, posX, posY + yOffset, statText, {255, 255, 255, 255}, 0.8f);
        yOffset += 15;
    }
}
