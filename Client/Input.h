#pragma once
#include "../Shared/InputCommand.h"

class Input
{
public:
    void OnKeyDown(unsigned int key);
    void OnKeyUp(unsigned int key);

    InputCommand ConsumeCommand();
    void Reset();

private:
    InputCommand command_{};

};