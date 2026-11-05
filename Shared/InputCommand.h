#pragma once
struct InputCommand
{
    // 상하좌우 키
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    bool firePressed = false;   // 총알 발사
};