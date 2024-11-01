#ifndef SCRIPT_H
#define SCRIPT_H

//TODO: change to ScriptBase
class Script
{

public:
    virtual void start() {}
    virtual void update() {}
    virtual ~Script() {}

    float deltaTime = 1.0f / 60.0f;
};

#endif
