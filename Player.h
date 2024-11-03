#pragma once
#include <iostream>
#include "Renderer.h"
#include "Physics.h"

class Player : public Sprite
{
public:
    Player(Renderer* renderer, const std::string& id)
        : Sprite (renderer, "C:/Users/abhis/gamedev/Shadow/fly.png", id)
    {
        std::cout << "Creating object: " << getId() << std::endl;
        auto controller = std::make_shared<TopDownCharacterControllerScript>(this, 5);
        setImage ("C:/Users/abhis/gamedev/Shadow/fly.png");
        attachScript (controller);        
        isInitialized = true;
    }
};