// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include <SDL2/SDL.h>

class FPSCounter 
{
public:
    FPSCounter() : start_time (SDL_GetTicks()), frame_count(0), fps(0.0f) {}

    float getFPS() 
    {
        frame_count++;
        Uint32 current_time = SDL_GetTicks();

        if (current_time - start_time >= 500) {  // Update every 500 ms
            fps = (static_cast<float>(frame_count) * 1000.0f) / static_cast<float>(current_time - start_time);
            frame_count = 0;
            start_time = current_time;
        }
        return fps;
    }

private:
    Uint32 start_time;
    int frame_count;
    float fps;
};
