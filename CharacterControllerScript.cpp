#include "CharacterControllerScript.h"

CharacterControllerScript::CharacterControllerScript(Sprite* sprite, int speed)
    : sprite (sprite), speed (speed)
{
}

void CharacterControllerScript::start()
{
    std::cout << "CharacterControllerScript started!" << std::endl;
}

void CharacterControllerScript::update()
{
    handleLifeSupport();
    handleMovement();
    handleRotation();
}
