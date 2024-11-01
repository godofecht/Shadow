#pragma once
#include "Renderer.h"

class Object
{
    virtual void renderAndRunScripts (Renderer* renderer) = 0;
};
