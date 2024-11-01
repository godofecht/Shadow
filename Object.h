#pragma once
#include "Renderer.h"

class Object
{
    std::string id;
public:

    std::string getId() const { return id; }
    void setId (const std::string& id) { this->id = id; }

    virtual void renderAndRunScripts (Renderer* renderer) = 0;
};
