#pragma once
#include <iostream>
#include "Renderer.h"
#include "Physics.h"

class Player : public SimpleSprite
{
public:
    Player (Renderer* renderer, const std::string& id)
        : SimpleSprite (renderer, "C:/Users/abhis/gamedev/Shadow/tankhead.png", id)
    {
        std::cout << "Creating object: " << getId() << std::endl;
        auto controller = std::make_shared<TopDownCharacterControllerScript>(this, 5);
        setImage ("C:/Users/abhis/gamedev/Shadow/tankbody.png");

        addPart ("C:/Users/abhis/gamedev/Shadow/tankhead.png", "head");
        getPart ("head")->setBounds (Rect<float> (0, 0, 100, 100));

        attachScript (controller);        
        isInitialized = true;
    }
};