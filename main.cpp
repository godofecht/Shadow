#include "SDLApp.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int main (int argc, char* args[])
{
    Game app ("BrainRot Engine", 700, 700);
    app.run();
    return 0;
}
