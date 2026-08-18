// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <iostream>
#include "Engine/Core/Geometry.h"

// Single SDL_ttf-backed text renderer for every platform. A previous
// Windows-only Direct2D/DirectWrite path here was dead code (its
// RenderTextToTexture/Col helpers were never called) and its drawTextToRenderer
// took a TTF_Font* while every call site passes a font path, so it could not
// compile once the rest of the tree was made warning-clean. SDL_ttf is
// available on all targets (vcpkg on Windows, ports on Emscripten, apt/brew
// natively), so one implementation serves them all.
class TextWriter
{
public:
    TextWriter (SDL_Renderer* _sdlRenderer) : sdlRenderer (_sdlRenderer)
    {
        (void)sdlRenderer;

        if (TTF_Init() != 0)
        {
            std::cerr << "SDL_ttf initialization failed: " << TTF_GetError() << '\n';
        }
    }

    ~TextWriter()
    {
        TTF_Quit();
    }

    void drawTextToRenderer(
        const std::wstring& text,
        SDL_Renderer* renderer,
        const Rect<float>& bounds,
        const std::string& fontPath,
        const SDL_Color& color = {0, 255, 0, 255})
    {
        // Convert std::wstring to UTF-8 for SDL_ttf. The engine only ever
        // passes ASCII labels, so a per-code-unit conversion is sufficient.
        // The explicit static_cast also avoids the implicit wchar_t -> char
        // narrowing that MSVC rejects under /WX (warning C4244).
        std::string utf8Text;
        utf8Text.reserve (text.size());
        for (const wchar_t wc : text)
        {
            utf8Text.push_back (static_cast<char> (wc));
        }

        // Load font (using a default system font path for macOS)
        TTF_Font* font = TTF_OpenFont(fontPath.c_str(), 24);
        if (!font)
        {
            if (!fontPath.empty() && fontPath[0] == '/')
            {
                font = TTF_OpenFont(fontPath.substr(1).c_str(), 24);
            }
            else
            {
                font = TTF_OpenFont(("/" + fontPath).c_str(), 24);
            }
        }
#ifdef __EMSCRIPTEN__
        if (!font)
        {
            font = TTF_OpenFont("default.ttf", 24);
        }
        if (!font)
        {
            font = TTF_OpenFont("/default.ttf", 24);
        }
#endif
        if (!font)
        {
#ifndef __EMSCRIPTEN__
            std::cerr << "Failed to load font: " << TTF_GetError() << '\n';
#endif
            return;
        }

        // Calculate the size of the text using SDL_ttf
        int textWidth, textHeight;
        if (TTF_SizeUTF8(font, utf8Text.c_str(), &textWidth, &textHeight) != 0)
        {
            std::cerr << "Failed to calculate text size: " << TTF_GetError() << '\n';
            TTF_CloseFont(font);
            return;
        }

        // Adjust bounds if necessary based on text size
        float xPosition = bounds.x;
        float yPosition = bounds.y;

        // Create the texture for text rendering
        SDL_Surface* textSurface = TTF_RenderUTF8_Blended(font, utf8Text.c_str(), color);
        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        SDL_FreeSurface(textSurface);
        TTF_CloseFont(font);

        // Set the destination rectangle for rendering (position and size)
        SDL_Rect dstRect;
        dstRect.x = static_cast<int>(xPosition);
        dstRect.y = static_cast<int>(yPosition);
        dstRect.w = textWidth;
        dstRect.h = textHeight;

        // Render the text texture to the SDL renderer
        SDL_RenderCopy(renderer, textTexture, nullptr, &dstRect);

        // Clean up texture after rendering
        SDL_DestroyTexture(textTexture);
    }

    void reset() { }

private:
    SDL_Renderer* sdlRenderer;
};
