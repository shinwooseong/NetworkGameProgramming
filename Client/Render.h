#pragma once

#include <windows.h>
#include <atlimage.h>

class Render
{
public:
    // 배경
    void DrawBackground(
        HDC dc,
        CImage& image,
        int width,
        int height,
        float scrollOffset);


    // 플레이어
    void DrawPlayer(HDC dc, CImage& image,
        float x, float y, int size, int hp, bool showSprite);


    // 적 그리기
    void DrawEnemy(HDC dc, CImage& image,
        int imageWidth, int imageHeight,
        float x, float y, float size, int hp);


    // 총알 및 피격 폭발
    void DrawSprite(HDC dc, CImage& image,
        float x, float y, float width, float height,
        int sourceWidth, int sourceHeight);

    // 적 공격 관련(운석, 빨간 원, 충돌 폭발)
    void DrawMeteorWarning(HDC dc, int x, int endY);

    // 안개
    void DrawFog(HDC dc, CImage& image, int width, int height, BYTE alpha);
    
    // 배경 전환
    void DrawFade(HDC dc, int width, int height, BYTE alpha);

    // 아이템 먹은 상태 표시
    void DrawStatusIcon(HDC dc, CImage& image,
        int x, int y, int size, int sourceWidth, int sourceHeight);

    // 일시정지(F1)
    void DrawPause(HDC dc, int width, int height);

    // 시작화면
    void DrawStartScreen(HDC dc, int width, int height,
        CImage& background, CImage& title, CImage& startButton,
        CImage& emptySlot, CImage& readySlot, const bool (&occupied)[2],
    int myPlayerNumber, int hostPlayerNumber);

    // 결과화면
    void DrawResultScreen(HDC dc, int width, int height,
        CImage& background, CImage& resultImage, float playTime, int score);
};