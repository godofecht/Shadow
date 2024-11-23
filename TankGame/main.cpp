
#define SDL_MAIN_HANDLED

#pragma once
#include "SDLApp.h"

#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>

int main (int argc, char* argv[])
{
    TopDownTileGame app ("Tank Example", 700, 700);
    app.run();
    return 0;
}