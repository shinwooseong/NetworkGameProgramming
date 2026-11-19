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




void Render::DrawPlayer(HDC dc, CImage& image,
    float x, float y, int size, int hp, bool showSprite)
{
    if (showSprite && !image.IsNull())
    {
        image.Draw(dc, static_cast<int>(x), static_cast<int>(y),
            size, size, 0, 0, image.GetWidth(), image.GetHeight());
    }

    wchar_t text[50];
    swprintf_s(text, _countof(text), L"player hp : %d", hp);
    TextOutW(dc, static_cast<int>(x + size),
        static_cast<int>(y + size), text, lstrlenW(text));

    const int hpMargin = 5;
    const RECT barBackground = {
        static_cast<LONG>(x),
        static_cast<LONG>(y + size + hpMargin),
        static_cast<LONG>(x + size),
        static_cast<LONG>(y + size + hpMargin * 2)
    };

    HBRUSH backgroundBrush = CreateSolidBrush(RGB(100, 100, 100));
    FillRect(dc, &barBackground, backgroundBrush);
    DeleteObject(backgroundBrush);

    RECT hpBar = barBackground;
    hpBar.right = static_cast<LONG>(x + hp * 10);

    HBRUSH hpBrush = CreateSolidBrush(RGB(255, 0, 0));
    FillRect(dc, &hpBar, hpBrush);
    DeleteObject(hpBrush);
}


void Render::DrawEnemy(HDC dc, CImage& image,
    int imageWidth, int imageHeight,
    float x, float y, float size, int hp)
{
    if (!image.IsNull())
    {
        image.Draw(dc, static_cast<int>(x), static_cast<int>(y),
            static_cast<int>(size), static_cast<int>(size),
            0, 0, imageWidth, imageHeight);
    }

    wchar_t text[50];
    swprintf_s(text, _countof(text), L"enemy hp : %d", hp);
    TextOutW(dc, static_cast<int>(x + size),
        static_cast<int>(y + size), text, lstrlenW(text));

    const int hpBarHeight = 20;
    const RECT barBackground = {
        static_cast<LONG>(x),
        static_cast<LONG>(y - hpBarHeight),
        static_cast<LONG>(x + size),
        static_cast<LONG>(y - hpBarHeight / 2)
    };

    HBRUSH backgroundBrush = CreateSolidBrush(RGB(100, 100, 100));
    FillRect(dc, &barBackground, backgroundBrush);
    DeleteObject(backgroundBrush);

    RECT hpBar = barBackground;
    hpBar.right = static_cast<LONG>(x + hp / 3);

    HBRUSH hpBrush = CreateSolidBrush(RGB(255, 0, 0));
    FillRect(dc, &hpBar, hpBrush);
    DeleteObject(hpBrush);
}


void Render::DrawSprite(HDC dc, CImage& image,
    float x, float y, float width, float height,
    int sourceWidth, int sourceHeight)
{
    if (image.IsNull())
        return;

    image.Draw(dc,
        static_cast<int>(x), static_cast<int>(y),
        static_cast<int>(width), static_cast<int>(height),
        0, 0, sourceWidth, sourceHeight);
}


void Render::DrawMeteorWarning(HDC dc, int x, int endY)
{
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
    if (pen == nullptr)
        return;

    HGDIOBJ previousPen = SelectObject(dc, pen);
    MoveToEx(dc, x, 0, nullptr);
    LineTo(dc, x, endY);
    SelectObject(dc, previousPen);
    DeleteObject(pen);
}


void Render::DrawFog(
    HDC dc, CImage& image, int width, int height, BYTE alpha)
{
    if (image.IsNull() || width <= 0 || height <= 0 || alpha == 0)
        return;

    HDC fogDC = image.GetDC();
    if (fogDC == nullptr)
        return;

    const BLENDFUNCTION blend = {
        AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA
    };

    AlphaBlend(dc, 0, 0, width, height,
        fogDC, 0, 0, image.GetWidth(), image.GetHeight(), blend);

    image.ReleaseDC();
}

void Render::DrawFade(HDC dc, int width, int height, BYTE alpha)
{
    if (width <= 0 || height <= 0 || alpha == 0)
        return;

    HDC fadeDC = CreateCompatibleDC(dc);
    if (fadeDC == nullptr)
        return;

    // 단색 픽셀 하나를 화면 전체로 늘려 그립니다.
    HBITMAP bitmap = CreateCompatibleBitmap(dc, 1, 1);
    if (bitmap == nullptr)
    {
        DeleteDC(fadeDC);
        return;
    }

    HGDIOBJ previous = SelectObject(fadeDC, bitmap);
    if (previous != nullptr)
    {
        SetPixelV(fadeDC, 0, 0, RGB(255, 255, 240));

        const BLENDFUNCTION blend = { AC_SRC_OVER, 0, alpha, 0 };
        AlphaBlend(dc, 0, 0, width, height,
            fadeDC, 0, 0, 1, 1, blend);

        SelectObject(fadeDC, previous);
    }

    DeleteObject(bitmap);
    DeleteDC(fadeDC);
}


void Render::DrawStatusIcon(HDC dc, CImage& image,
    int x, int y, int size, int sourceWidth, int sourceHeight)
{
    const int margin = 5;
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(255, 255, 200));

    if (pen != nullptr)
    {
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

        Rectangle(dc, x - margin, y - margin,
            x + size + margin, y + size + margin);

        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }

    DrawSprite(dc, image, x, y, size, size, sourceWidth, sourceHeight);
}

void Render::DrawPause(HDC dc, int width, int height)
{
    const int centerX = width / 2;
    const int centerY = height / 2;
    RECT box = { centerX - 100, centerY - 50,
                 centerX + 100, centerY + 50 };

    HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    if (pen != nullptr)
    {
        HGDIOBJ oldPen = SelectObject(dc, pen);
        Rectangle(dc, box.left, box.top, box.right, box.bottom);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }

    const int oldMode = SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, L"GamePause", -1, &box,
        DT_VCENTER | DT_SINGLELINE | DT_CENTER);
    SetBkMode(dc, oldMode);
}



void Render::DrawStartScreen(HDC dc, int width, int height,
    CImage& background, CImage& title, CImage& startButton,
    CImage& emptySlot, CImage& readySlot, unsigned int playerSlots)
{
    const int centerX = width / 2;
    const int centerY = height / 2;

    if (!background.IsNull())
    {
        DrawSprite(dc, background, 0, 0, width, height,
            background.GetWidth(), background.GetHeight());
    }

    if (!title.IsNull())
    {
        DrawSprite(dc, title, centerX - 150, centerY - 135, 300, 90,
            title.GetWidth(), title.GetHeight());
    }

    if (!startButton.IsNull())
    {
        DrawSprite(dc, startButton, centerX - 150, centerY - 80, 300, 200,
            startButton.GetWidth(), startButton.GetHeight());
    }

    const wchar_t message[] = L"Click 'START' TO GAME START";
    const int oldMode = SetBkMode(dc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(dc, RGB(255, 255, 255));

    TextOutW(dc, centerX - 100, centerY - 100, message, lstrlenW(message));

    const wchar_t* slotLabels[] = {L"PLAYER 1", L"PLAYER 2"};
    const int slotCenters[] = {centerX - 70, centerX + 70};
    constexpr int slotSize = 64;
    const int slotTop = centerY + 120;

    SetTextColor(dc, RGB(0, 0, 0));
    
    
    for (int i = 0; i < 2; ++i)
    {
        const int slotX = slotCenters[i];
       
        const bool hasPlayer = (playerSlots & (1u << i)) != 0; // 현재 확인 중인 i번 자리에 플레이어 있는지 확인
        CImage& slotImage = hasPlayer ? readySlot : emptySlot;

        if (!slotImage.IsNull())
        {
            DrawSprite(dc, slotImage, slotX - slotSize / 2, slotTop,
                slotSize, slotSize,
                slotImage.GetWidth(), slotImage.GetHeight());
        }   
    }

    SetTextColor(dc, oldColor);
    SetBkMode(dc, oldMode);
}


void Render::DrawResultScreen(HDC dc, int width, int height,
    CImage& background, CImage& resultImage, float playTime, int score)
{
    const int centerX = width / 2;
    const int centerY = height / 2;

    if (!background.IsNull())
    {
        DrawSprite(dc, background, 0, 0, width, height,
            background.GetWidth(), background.GetHeight());
    }

    if (!resultImage.IsNull())
    {
        DrawSprite(dc, resultImage,
            centerX - 150, centerY - 100, 300, 200,
            resultImage.GetWidth(), resultImage.GetHeight());
    }

    const int oldMode = SetBkMode(dc, TRANSPARENT);

    const wchar_t message[] = L"Press Enter to Restart / Esc to Quit";
    TextOutW(dc, centerX - 110, centerY + 50, message, lstrlenW(message));

    wchar_t timeText[64];
    swprintf_s(timeText, _countof(timeText),
        L"Your PlayTime: %.1f sec", playTime);
    TextOutW(dc, centerX - 70, centerY + 80, timeText, lstrlenW(timeText));

    wchar_t scoreText[64];
    swprintf_s(scoreText, _countof(scoreText), L"Your Score: %d", score);
    TextOutW(dc, centerX - 60, centerY + 110, scoreText, lstrlenW(scoreText));

    SetBkMode(dc, oldMode);
}