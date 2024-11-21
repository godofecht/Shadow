#pragma once
#include <iostream>
#include "Renderer.h"
#include "Physics.h"

class Player : public SimpleSprite
{
public:
    Player (Renderer* renderer, const std::string& id)
        : SimpleSprite (renderer, id)
    {
        std::cout << "Creating object: " << getId() << std::endl;
        auto controller = std::make_shared<TopDownCharacterControllerScript>(this, 5);
        setImage ("C:/Users/abhis/gamedev/Shadow/tankbody.png");
        addPart ("C:/Users/abhis/gamedev/Shadow/tankhead.png", "head");
        setPartBounds (Rect<float> (0, 0, 100, 100), "head");

        auto head = addPart ("C:/Users/abhis/gamedev/Shadow/tankhead.png", "head");
        head->setBounds (Rect<float> (0, 0, 100, 100));

        attachScript (controller);        
        isInitialized = true;
    }

    void setPartBounds (const Rect<float>& bounds, const std::string& id)
    {
        auto part = getPart (id);
        if (part == nullptr)
        {
            std::cerr << "Part not found: " << id << std::endl;
            return;
        }
        part->setBounds (bounds);
    }
};