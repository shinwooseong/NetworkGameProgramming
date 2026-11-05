#pragma once

#include <windows.h>
#include <atlimage.h>

class Render
{
public:
    void DrawBackground(
        HDC dc,
        CImage& image,
        int width,
        int height,
        float scrollOffset);
};