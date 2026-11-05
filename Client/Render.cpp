#include "Render.h"

void Render::DrawBackground(
    HDC dc,
    CImage& image,
    int width,
    int height,
    float scrollOffset)
{
    if (image.IsNull() || width <= 0 || height <= 0)
        return;

    const int back_height = image.GetHeight();
    const int back_width = image.GetWidth();

    if (back_height <= 0 || back_width <= 0)
        return;

    int scrollY = static_cast<int>(scrollOffset) % back_height;

    if (scrollY > 0)
        scrollY -= back_height;

    for (int y = -back_height;
        y < height + back_height;
        y += back_height)
    {
        image.Draw(
            dc,
            0, y + scrollY, width, back_height,
            0, 0, back_width, back_height);
    }
}