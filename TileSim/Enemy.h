#pragma once
#include <iostream>
#include "Renderer.h"

class Enemy : public SimpleSprite
{
public:
    Enemy (Renderer* renderer, const std::string& id)
        : SimpleSprite (renderer, "C:/Users/abhis/gamedev/Shadow/fly.png", id)
    {
        std::cout << "Creating object: " << getId() << std::endl;
        // auto controller = std::make_shared<EnemyControllerScript>(this, 5);
        setImage ("C:/Users/abhis/gamedev/Shadow/fly.png");
        // attachScript (controller);        
        isInitialized = true;
    }
};

// class Enemy : public Sprite
// {
// public:
//     Enemy(Renderer* renderer, const std::string& id)
//         : Sprite(renderer, "", id), health(health)
//     {
//         setPosition(startX, startY);
//         setSize(40, 40); // Set default enemy size
//         std::cout << "Enemy created: " << id << " at position (" << startX << ", " << startY << ")" << std::endl;
//     }

//     void setTarget(Sprite* target)
//     {
//         this->target = target;
//     }

//     // virtual void update() override
//     // {
//     //     if (health <= 0)
//     //     {
//     //         destroy();
//     //         return;
//     //     }

//     //     // Basic movement: follow target if one is set
//     //     if (target != nullptr)
//     //     {
//     //         moveTowardsTarget();
//     //     }
//     // }

//     void takeDamage(int amount)
//     {
//         health -= amount;
//         std::cout << "Enemy " << getId() << " took " << amount << " damage. Health: " << health << std::endl;
//         if (health <= 0)
//         {
//             std::cout << "Enemy " << getId() << " has been destroyed." << std::endl;
//             destroy();
//         }
//     }

//     int getHealth() const { return health; }

// private:
//     int startX = 0;
//     int startY = 0;
//     int health = 100;
//     Sprite* target = nullptr;

//     void moveTowardsTarget()
//     {
//         if (target == nullptr)
//         {
//             return;
//         }

//         float targetX, targetY;
//         target->getPosition(targetX, targetY);

//         float enemyX, enemyY;
//         getPosition(enemyX, enemyY);

//         float deltaX = targetX - enemyX;
//         float deltaY = targetY - enemyY;

//         // Normalize the direction vector and move towards the target
//         float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
//         if (length != 0)
//         {
//             deltaX /= length;
//             deltaY /= length;
//         }

//         float moveSpeed = 1.0f; // Set movement speed
//         setPosition(enemyX + deltaX * moveSpeed, enemyY + deltaY * moveSpeed);
//     }
// };
