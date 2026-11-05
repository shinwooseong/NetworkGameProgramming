#include "Input.h"
#include <windows.h>

void Input::OnKeyDown(unsigned int key)
{
    switch (key)
    {
    case VK_UP:
        command_.up = true;
        break;

    case VK_DOWN:
        command_.down = true;
        break;

    case VK_LEFT:
        command_.left = true;
        break;

    case VK_RIGHT:
        command_.right = true;
        break;

    case VK_SPACE:
        command_.firePressed = true;
        break;

    }
}

void Input::OnKeyUp(unsigned int key)
{
    switch (key)
    {
    case VK_UP:
        command_.up = false;
        break;

    case VK_DOWN:
        command_.down = false;
        break;

    case VK_LEFT:
        command_.left = false;
        break;

    case VK_RIGHT:
        command_.right = false;
        break;

    }
}

InputCommand Input::ConsumeCommand()
{
    // 지금까지 기록한 입력을 복사합니다.
    InputCommand result = command_;

    // 발사 요청은 한 번만 전달합니다.
    command_.firePressed = false;

    // 방향키 상태는 유지합니다.
    return result;
}

void Input::Reset()
{
    command_ = {};

}