#include "Assets.h"
#include <windows.h>
#include <tchar.h>
#include <time.h>
#include <stdbool.h>
#include <atlImage.h>
#include <windowsx.h>	// AlphaBlend() 함수 이용
#include <mmsystem.h>	

#include "Client/Input.h"


#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

#define PIE 3.14159265
#define MAX_BULLET 10
#define MAX_REDCIRCLE 10
#define MAX_ITEM 15
#define MAX_ITEM_EFFECT 15

const float PLAYER_MOVE_RATIO = 17.0f / 30.0f;
const float OBJECT_MOVE_RATIO = 17.0f / 31.0f;


// Report actual paint intervals to the Visual Studio Debug output window.
void ReportFps(bool playing)
{
    static LARGE_INTEGER frequency = {};
    static LARGE_INTEGER previous = {};
    static bool tracking = false;
    static double elapsed = 0.0;
    static double longestFrame = 0.0;
    static int frames = 0;

    if (frequency.QuadPart == 0)
        QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    if (!playing || !tracking)
    {
        elapsed = 0.0;
        longestFrame = 0.0;
        frames = 0;
    }
    else
    {
        double seconds = (double)(now.QuadPart - previous.QuadPart)
            / (double)frequency.QuadPart;
        elapsed += seconds;
        ++frames;
        if (seconds > longestFrame)
            longestFrame = seconds;

        if (elapsed >= 1.0)
        {
            wchar_t message[128];
            swprintf_s(message, _countof(message),
                L"[FPS] %.1f | Max frame: %.1f ms\n",
                frames / elapsed, longestFrame * 1000.0);
            OutputDebugStringW(message);
            elapsed = 0.0;
            longestFrame = 0.0;
            frames = 0;
        }
    }

    previous = now;
    tracking = playing;
}

HINSTANCE g_hInst;
LPCTSTR lpszClass = L"Final Project";
LPCTSTR lpszWindowName = L"Final Project";
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpszCmdParam, int nCmdShow)
{
	HWND hWnd;
	MSG Message;
	WNDCLASSEX WndClass;
	g_hInst = hInstance;
	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = (WNDPROC)WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = lpszClass;
	WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&WndClass);
	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW,
		0, 0, 800, 700, NULL, (HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	// Rendering has its own 60 Hz clock. Gameplay timers remain unchanged.
	LARGE_INTEGER frequency, now;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&now);
	const double frameTicks = (double)frequency.QuadPart / 60.0;
	double nextFrame = (double)now.QuadPart + frameTicks;

	// High-resolution waiting without changing the system timer resolution.
	HANDLE frameTimer = CreateWaitableTimerExW(NULL, NULL, 0x00000002,
		TIMER_MODIFY_STATE | SYNCHRONIZE);
	if (!frameTimer)
		frameTimer = CreateWaitableTimerW(NULL, FALSE, NULL);

	bool running = true;
	while (running)
	{
		while (PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
		{
			if (Message.message == WM_QUIT)
			{
				running = false;
				break;
			}
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
		if (!running)
			break;

		QueryPerformanceCounter(&now);
		if ((double)now.QuadPart >= nextFrame)
		{
			if (!IsIconic(hWnd))
			{
				InvalidateRect(hWnd, NULL, FALSE);
				UpdateWindow(hWnd);
			}

			nextFrame += frameTicks;
			QueryPerformanceCounter(&now);
			// Skip missed frames instead of drawing a burst after a stall.
			if (nextFrame <= (double)now.QuadPart)
				nextFrame = (double)now.QuadPart + frameTicks;
		}

		double remaining = (nextFrame - (double)now.QuadPart)
			/ (double)frequency.QuadPart;
		if (remaining <= 0.0)
			continue;

		LARGE_INTEGER due;
		due.QuadPart = -(LONGLONG)(remaining * 10000000.0 + 1.0);
		if (frameTimer && SetWaitableTimer(frameTimer, &due, 0, NULL, NULL, FALSE))
		{
			MsgWaitForMultipleObjectsEx(1, &frameTimer, INFINITE,
				QS_ALLINPUT, MWMO_INPUTAVAILABLE);
		}
		else
		{
			DWORD waitMs = (DWORD)(remaining * 1000.0);
			MsgWaitForMultipleObjectsEx(0, NULL, waitMs > 0 ? waitMs : 1,
				QS_ALLINPUT, MWMO_INPUTAVAILABLE);
		}
	}
	if (frameTimer)
		CloseHandle(frameTimer);
	return (int)Message.wParam;
}
RECT client; // 클라이언트 크기 가져오는 전역변수

int score = 0;            // 게임 점수

HBITMAP backBitmap = NULL; // 배경 비트맵
CImage back[2]; // 배경 이미지
int backX, backY; // 배경 이미지 사이즈
int current_back = 0;	// 처음은 낮배경부터 , 0 = 낮 , 1= 밤
int back_scrollX = 0;		// 배경 수평 스크롤
float back_scrollY = 0.0f;		// 배경 수직 스크롤
const int back_scroll_speed = 5;		// 배경 움직이는 속도

CImage GameStartBGImage;
int GameStartBGImageW, GameStartBGImageH;
CImage GameEndBGImage;
int GameEndBGImageW, GameEndBGImageH;
CImage GameClearBGImage;
int GameClearBGImageW, GameClearBGImageH;

CImage GameStartImage;
int startImageW, startImageH;

CImage GameOverImage;
int overImageW, overImageH;

CImage TextImage;
int textW, textH;

CImage buttonImage;
int buttonW, buttonH;


CImage playerImage; // 플레이어 이미지
int playerImageX, playerImageY; // 이미지 사이즈
typedef struct Player
{
	float x, y;
	int size;

	int hp;

	bool revive;
	int reviveAnimation;
}Player;
Player player;

float player_speed = 5.0f;	// 플레이어 속도
int speed_up_timer = 0;		// 속도 증가 아이템 먹은 시간 체크

bool IsGetItem = false; // 아이템 먹을시 깜빡거림

CImage enemyImage; // 적 이미지
CImage enemyImage2;
CImage enemyImage3;
int enemyImageX, enemyImageY; // 적 이미지 사이즈
typedef struct Enemy
{
	float x, y; // 위치
	float size; // 크기
	float dx, dy; // 방향 변수
	float speed; // 속도
	int directionChangeCounter; // 랜덤 방향 이동을 위한 변수

	int hp; //
}Enemy;
Enemy enemy; // 보스 선언



CImage bulletImage; // 총알 이미지
int bulletImageX, bulletImageY; // 총알 이미지 사이즈

typedef struct Bullet
{
	float x, y;
	float size;
	float speed;
	bool active; // 총알 활성화 ( active시 출력 )

	bool explosion; // true시 폭발 이미지 출력
	float exploX, exploY; // 폭발 이미지 크기
	int explosionCnt = 0; // 애니메이션
}Bullet;
Bullet bullet[MAX_BULLET]; // 총알 개수
int bulletCnt = 0;

CImage explosionImage[4];
int explosionImageX, explosionImageY;

CImage playerExplo[4];
int playerExW, playerExH;


CImage redcircleImage;
int redcircleImageW, redcircleImageH;

typedef struct Obstacle
{
	float size;
	float x, y;
	float dx, dy;
	float speed;
	bool active;

	bool explosion;
	float exploX, exploY; // 폭발 이미지 크기
	int explosionCnt = 0;
	//int frame; // 폭발지속 시간

}Obstacle;

Obstacle redcircle[MAX_REDCIRCLE]; // 보스가 쏘는 장애물


CImage meteorImage[3];
int meteorImageW, meteorImageH;

typedef struct Meteor
{
	float size;
	float x, y;
	bool lineActive;
	bool meteorActive;
	int animationCnt;

	int lineY;
	int delayCnt;

	bool explosion;
	float exploX, exploY;
	int explosionCnt = 0;



}Meteor;

Meteor meteor; // 스테이지 2 운석 장애물




// 아이템들 종류
typedef enum ItemType
{
	ITEM_HEAL,		// 힐팩
	ITEM_SPEED,		// 속도 증가
	ITEM_POSION,	// 독
	ITEM_FOG,		// 안개
	ITEM_REBIRTH	// 부활
}ItemType;

CImage itemImages[5];
int item_image_W[5], item_image_H[5];

// 아이템 슬롯 이미지 정보
CImage ItemSlotImage;
int ItemSlotImageW, ItemSlotImageH;

// 아이템 구조체
typedef struct Item
{
	float x, y;		// 생성 위치
	float size;		// 아이템 크기
	bool active;	// 아이템 활성화
	ItemType type;	// 아이템 종류
}Item;

Item items[MAX_ITEM];

// 아이템 등장시 이펙트
typedef struct ItemEffect
{
	float x, y;
	bool active;
	int animation_count;	// 애니메이션 프레임
	int duration;	// 이펙트 지속시간
}ItemEffect;
CImage item_effectImage[4];


ItemEffect itemEffects[MAX_ITEM_EFFECT];

CImage reviveImage[3];
int reviveImageW, reviveImageH;

CImage fogImage;
int fog_image_W, fog_image_H;
bool fog_active = FALSE;
int fog_timer = 0;
bool has_rebirth = FALSE;

void MoveEnemy()
{
	// 적 이동
	enemy.x += enemy.speed * enemy.dx * OBJECT_MOVE_RATIO;
	enemy.y += enemy.speed * enemy.dy * OBJECT_MOVE_RATIO;


	// 랜덤 이동
	enemy.directionChangeCounter++; // 50 이상되면 보스의 방향이 랜덤으로 바뀜
	if (enemy.directionChangeCounter > 50) {
		float angle = (float)(rand() % 360) * PIE / 180.0;
		enemy.dx = cos(angle);
		enemy.dy = sin(angle);
		enemy.directionChangeCounter = 0;
	}


	// 화면 가장자리 튕기기
	if (enemy.x < 0 || enemy.x + enemy.size > client.right) enemy.dx *= -1;
	if (enemy.y < 0 || enemy.y + enemy.size > 200) enemy.dy *= -1;
}
void ShotBullet()
{
	int move = 15; // 15만큼 이동
	for (int i = 0; i < MAX_BULLET; ++i)
	{
		if (bullet[i].active) bullet[i].y -= bullet[i].speed * move * OBJECT_MOVE_RATIO; // active시 좌표이동 => spacebar 입력 !!
		if (bullet[i].y < 0) bullet[i].active = false; // 총알 화면 밖으로 나갈시 초기화
	}
}
int CheckEnemyBulletCollision()
{
	for (int i = 0; i < MAX_BULLET; ++i)
	{
		int marin = 40; // 히트박스 조정할 정도,,
		RECT rBullet = { bullet[i].x,bullet[i].y,bullet[i].x + bullet[i].size,bullet[i].y + bullet[i].size };
		RECT rEnemy = { enemy.x,enemy.y,enemy.x + enemy.size,enemy.y + enemy.size - marin }; // marin 변수로 히트박스를 줄이기
		RECT intersect;
		if (IntersectRect(&intersect, &rBullet, &rEnemy))
		{
			enemy.hp--;
			score += 10;
			bullet[i].active = false; // 총알 초기화
			bullet[i].explosion = true; // 폭발 !!
			bullet[i].exploX = bullet[i].x; bullet[i].exploY = bullet[i].y;

			// * 보스 타격음
			mciSendString(L"play hitSound from 0", NULL, 0, NULL);

			if (enemy.hp == 0) // 게임종료
			{
				score += 100;
				return 1;
			}
		}
	}

	for (int i = 0; i < MAX_BULLET; ++i)
	{
		if (bullet[i].explosion)
		{
			bullet[i].explosionCnt++;
			if (bullet[i].explosionCnt == 3)
			{
				bullet[i].explosionCnt = 0;
				bullet[i].explosion = false;
				bullet[i].exploX = 0; bullet[i].exploY = 0;
			}
		}
	}

	return 0;
}

int CheckRedcirclePlayerCollision()
{
	for (int i = 0; i < MAX_REDCIRCLE; ++i)
	{
		int marin = 20;
		RECT rRedcircle = { redcircle[i].x,redcircle[i].y ,redcircle[i].x + redcircle[i].size ,redcircle[i].y + redcircle[i].size };
		RECT rPlayer = { player.x + marin, player.y + marin,player.x + player.size - marin, player.y + player.size - marin };
		RECT intersectRect;
		if (IntersectRect(&intersectRect, &rRedcircle, &rPlayer) && redcircle[i].active)
		{
			player.hp--;

			// *플레이어 타격음
			mciSendString(L"play hurtSound from 0", NULL, 0, NULL);


			// 부활 처리
			if (has_rebirth && player.hp <= 0)
			{
				player.hp = 5;
				has_rebirth = FALSE;
				// 부활 이펙트 반영
				player.revive = true;
				player.reviveAnimation = 0;

				mciSendString(L"play reviveSound from 0", NULL, 0, NULL);
			}

			// 게임종료 !!!!!!!!!!!
			if (!has_rebirth && player.hp <= 0)
			{
				return 1;
			}

			redcircle[i].active = false;
			if (redcircle[i].active == false) // 충돌시 한번 작동
			{
				redcircle[i].explosion = true;
				redcircle[i].exploX = redcircle[i].x; redcircle[i].exploY = redcircle[i].y;
				redcircle[i].explosionCnt = 0;
				redcircle[i].x = enemy.x + enemy.size / 2;
				redcircle[i].y = enemy.y + enemy.size;
			}
		}
	}
	return 0;
}

int CheckMeteorPlayerCollision()
{
	int marin = 20;
	RECT rMeteor = { meteor.x + marin,meteor.y + marin, meteor.x + meteor.size - marin, meteor.y + meteor.size - marin };
	RECT rPlayer = { player.x, player.y,player.x + player.size, player.y + player.size };
	RECT intersectRect;
	if (IntersectRect(&intersectRect, &rMeteor, &rPlayer) && meteor.meteorActive)
	{
		player.hp--;

		// *플레이어 타격음
		mciSendString(L"play hurtSound from 0", NULL, 0, NULL);


		if (has_rebirth && player.hp <= 0)
		{
			player.hp = 5;
			has_rebirth = FALSE;
			// 부활이펙트 반영
			player.revive = true;
			player.reviveAnimation = 0;

			// *부활 효과음
			mciSendString(L"play reviveSound from 0", NULL, 0, NULL);
		}

		// 게임종료 !!!!!!!!!!!
		if (!has_rebirth && player.hp <= 0)
		{
			return 1;
		}

		meteor.meteorActive = false;
		if (meteor.meteorActive == false)
		{
			meteor.lineActive = false;
			meteor.lineY = 0;
			meteor.explosion = true;
			meteor.exploX = meteor.x + (meteor.size / 2), meteor.exploY = meteor.y;
			meteor.explosionCnt = 0;
			meteor.y = 0;
		}
	}
	return 0;
}

void playerExplosion() // 플레이어 충돌 폭발 애니메이션
{
	for (int i = 0; i < MAX_REDCIRCLE; ++i)
	{
		if (redcircle[i].explosion)
		{
			redcircle[i].explosionCnt++;
			if (redcircle[i].explosionCnt == 3)
			{
				redcircle[i].explosionCnt = 0;
				redcircle[i].explosion = false;
				redcircle[i].exploX = 0; redcircle[i].exploY = 0;
			}
		}
	}

	if (meteor.explosion)
	{
		meteor.explosionCnt++;
		if (meteor.explosionCnt == 3)
		{
			meteor.explosionCnt = 0;
			meteor.explosion = false;
			meteor.exploX = 0, meteor.exploY = 0;
		}
	}
}

void SpawnRedcircle()
{
	for (int i = 0; i < MAX_REDCIRCLE; ++i)
	{
		if (redcircle[i].active == false)
		{
			redcircle[i].active = true;

			redcircle[i].x = enemy.x + enemy.size / 2 - 10;
			redcircle[i].y = enemy.y + enemy.size - 10;

			// 플레이어쪽 방향계산해서 플레이어방향으로 쏘기
			float vx = (player.x + player.size / 2) - redcircle[i].x;
			float vy = (player.y + player.size / 2) - redcircle[i].y;
			float mag = sqrtf(vx * vx + vy * vy);
			redcircle[i].dx = vx / mag;
			redcircle[i].dy = vy / mag;

			break; // 하나씩 쏘기 
		}
	}
}

void MoveRedCircle()
{
	for (int i = 0; i < MAX_REDCIRCLE; ++i)
	{
		if (redcircle[i].active)
		{
			redcircle[i].x += redcircle[i].speed * redcircle[i].dx * OBJECT_MOVE_RATIO;
			redcircle[i].y += redcircle[i].speed * redcircle[i].dy * OBJECT_MOVE_RATIO;
		}

		if (redcircle[i].x > client.right || redcircle[i].x < client.left || redcircle[i].y > client.bottom)
			redcircle[i].active = false;
	}
}



void MoveMeteor()
{
	if (meteor.meteorActive)
	{
		meteor.y += 10.0f * OBJECT_MOVE_RATIO;
		meteor.animationCnt++;
		meteor.animationCnt = meteor.animationCnt % 3; // 0 1 2
	}

	if (meteor.y >= client.bottom)
	{
		meteor.meteorActive = false;
		meteor.lineActive = false;
		meteor.y = 0;
		meteor.lineY = 0;
	}
}




// 보스 체력에 따른 배경변화!
// 일정 체력에 도달 시 바로 배경이 변하면 부자연스러워서
// AlphaBlend() 함수 이용하여서 자연스럽게 구현해보았습니다!
bool background_changed = FALSE;
bool start_fade = FALSE;
bool end_fade = FALSE;
int fade_alpha = 0;

void Change_Background_Fade(HWND hWnd)
{
	if (!background_changed && !start_fade && enemy.hp <= 200)	// 체력 도달
	{
		start_fade = true;
		end_fade = true;

		mciSendString(L"play changeSound from 0", NULL, 0, NULL);
	}

	if (start_fade)
	{
		// 페이드 아웃(어두워지기)
		if (end_fade)
		{
			fade_alpha += 5;	// 화면 변화 속도
			if (fade_alpha >= 255)
			{
				fade_alpha = 255;
				current_back = 1;
				end_fade = FALSE;
			}
		}
		// 페이드 인(밝아지기)
		else
		{
			fade_alpha -= 5;
			if (fade_alpha <= 0)
			{
				fade_alpha = 0;
				start_fade = false;
				background_changed = true;
			}
		}
	}
}

// 아이템 초기화
void Init_Items()
{
	for (int i = 0; i < MAX_ITEM; ++i)
	{
		items[i].active = FALSE;
		items[i].x = 0;
		items[i].y = 0;
		items[i].size = 100.0f;
		items[i].type = ITEM_HEAL;
	}
}
// 아이템 생성
void Spawn_Item()
{
	for (int i = 0; i < MAX_ITEM; ++i)
	{
		if (!items[i].active && !itemEffects[i].active)
		{
			float x = (float)(rand() % (client.right - 50));
			float y = (float)(rand() % (client.bottom - 100));

			// 아이템에 위치만 저장
			items[i].x = x;
			items[i].y = y;
			items[i].size = 100.0f;
			items[i].active = FALSE;

			int probability = rand() % 100;
			if (probability < 20) { items[i].type = ITEM_HEAL; }
			else if (probability < 45) { items[i].type = ITEM_SPEED; }
			else if (probability < 50) { items[i].type = ITEM_REBIRTH; }
			else if (probability < 85) { items[i].type = ITEM_POSION; }
			else items[i].type = ITEM_FOG;

			// 이펙트 활성화
			itemEffects[i].x = x;
			itemEffects[i].y = y;
			itemEffects[i].active = TRUE;
			itemEffects[i].animation_count = 0;
			itemEffects[i].duration = 30; // 3초


			break;
		}
	}
}
// 아이템 이동 및 제거
void Update_Items()
{
	const float item_fall_speed = 2.0f;

	for (int i = 0; i < MAX_ITEM; ++i)
	{
		if (!items[i].active)
		{
			continue;
		}

		// 아이템은 화면 스크롤 되는 방향(위에서 아래로) 이동하게 했어요!
		items[i].y += item_fall_speed * OBJECT_MOVE_RATIO;

		// 화면 밖으로 나가면 비활성
		if (items[i].y > client.bottom)
		{
			items[i].active = FALSE;
		}
	}
}

// 아이템 먹고 화면 위 표시 기능
bool is_speed_active = FALSE;
bool is_fog_active = FALSE;
bool is_rebirth_ready = FALSE;

int fog_alpha = 0;
bool fog_fade_in = FALSE;
bool fog_fade_out = FALSE;
int Apply_Item_Effect(ItemType type)
{
	switch (type)
	{
	case ITEM_HEAL:
	{
		if (player.hp < 10)
		{
			player.hp += 2;
		}
		mciSendString(L"play healSound from 0", NULL, 0, NULL);
	}
	break;

	case ITEM_SPEED:
	{
		player_speed = 12.5f;
		speed_up_timer = 50;
		is_speed_active = TRUE;
		mciSendString(L"play speedSound from 0", NULL, 0, NULL);
	}
	break;

	case ITEM_POSION:
	{
		player.hp -= 2;
		if (player.hp <= 0 && !has_rebirth)
		{
			return 1;
		}
		mciSendString(L"play posionSound from 0", NULL, 0, NULL);
	}
	break;
	case ITEM_FOG:
	{
		fog_fade_in = TRUE;
		fog_alpha = 0;
		fog_timer = 50;
		is_fog_active = TRUE;
		mciSendString(L"play fogSound from 0", NULL, 0, NULL);
	}
	break;
	case ITEM_REBIRTH:
	{
		has_rebirth = TRUE;
		is_rebirth_ready = TRUE;
		mciSendString(L"play healSound from 0", NULL, 0, NULL);
	}
	break;
	}
	return 0;
}


// 플레이어가 아이템 먹음
int Check_Item_Player_Collision()
{

	for (int i = 0; i < MAX_ITEM; ++i)
	{
		if (!items[i].active)
		{
			continue;
		}

		RECT item_rect = { (int)items[i].x, (int)items[i].y, (int)(items[i].x + items[i].size), (int)(items[i].y + items[i].size) };

		int margin = 10;	// 플레이어가 아이템 가까이서 먹을 수 있게 하기 위한 여유 공간
		RECT player_rect = { player.x + margin, player.y + margin, player.x + player.size - margin, player.y + player.size - margin };

		RECT intersect;

		// 아이템 먹으면
		if (IntersectRect(&intersect, &item_rect, &player_rect))
		{
			int gameover = Apply_Item_Effect(items[i].type);
			items[i].active = false;
			IsGetItem = true;
			score += 30;

			if (gameover == 1)
			{
				return 1;  // 게임오버 신호 반환
			}
		}
	}
	return 0;
}


void Draw_Glow_Effect(HDC mdc, int x, int y, int icon_size)
{
	const int margin = 5; // 테두리 여유
	RECT outlineRect = { x - margin, y - margin, x + icon_size + margin, y + icon_size + margin };

	// 밝은 색의 펜 (흰색 또는 연노랑색 등)
	HPEN brightPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 200)); // 밝은 색 테두리
	HPEN oldPen = (HPEN)SelectObject(mdc, brightPen);

	HBRUSH oldBrush = (HBRUSH)SelectObject(mdc, GetStockObject(NULL_BRUSH)); // 내부 채우지 않음

	Rectangle(mdc, outlineRect.left, outlineRect.top, outlineRect.right, outlineRect.bottom);

	SelectObject(mdc, oldBrush);
	SelectObject(mdc, oldPen);
	DeleteObject(brightPen);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc, mdc;

	static HBITMAP hBitmap;

	static Input input;


	static bool bGameStart; // 게임시작을 위한 변수 => START BOX 누를시 시작
	static RECT startBox; // START BOX 
	static bool bGamePause; // 게임 일시정지
	static bool bGameOver; // 게임종료
	static bool bGameClear; // 보스 쓰러뜨리며 게임종료

	static float GameTime;
	static int redcircleCounter; // 보스가 빨간원을 발사하는 시계
	static int meteorCounter; // 운석 생성 시계

	static float item_counter;	// 아이템 생성 시계

	static int blinkCnt; // 플레이어 깜빡거림 시계

	static int reviveCnt; // 부활시계

	switch (iMessage) {
	case WM_ERASEBKGND:
		// 배경 지워서 계속 깜빡임
		// 그래서 직접 그리게 해서 깜빡임 방지하게 하기!
		return 1;
	case WM_CREATE: {
		srand((unsigned)time(NULL));
		GetClientRect(hWnd, &client);

		// 게임 시작화면 배경음악
		PlaySound(Assets::startscreen_bgm().c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);

		// 효과음 미리 열어두기
		mciSendString((L"open \"" + Assets::shot() + L"\" alias shotSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::hurt() + L"\" alias hurtSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::win() + L"\" alias winSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::heal() + L"\" alias healSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::posion() + L"\" alias posionSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::speed() + L"\" alias speedSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::revive() + L"\" alias reviveSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::hit() + L"\" alias hitSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::fadechange() + L"\" alias changeSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::meteor() + L"\" alias meteorSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::fogSound() + L"\" alias fogSound").c_str(), NULL, 0, NULL);
		mciSendString((L"open \"" + Assets::lose() + L"\" alias loseSound").c_str(), NULL, 0, NULL);


		// 텍스트 및 테두리 이미지 정보 로드
		TextImage.Load(Assets::text().c_str());
		textW = TextImage.GetWidth(), textH = TextImage.GetHeight();

		// 게임 시작 전 배경이미지
		GameStartBGImage.Load(Assets::gamestartbg().c_str());
		GameStartBGImageW = GameStartBGImage.GetWidth(), GameStartBGImageH = GameStartBGImage.GetHeight();

		GameStartImage.Load(Assets::gamestart().c_str());
		startImageW = GameStartImage.GetWidth(), startImageH = GameStartImage.GetHeight();

		// 게임 앤드 배경이미지
		GameEndBGImage.Load(Assets::gameendbg().c_str());
		GameEndBGImageW = GameEndBGImage.GetWidth(), GameEndBGImageH = GameEndBGImage.GetHeight();

		GameOverImage.Load(Assets::gameover().c_str());
		overImageW = GameOverImage.GetWidth(), overImageH = GameOverImage.GetHeight();
		GameClearBGImage.Load(Assets::youwin().c_str());
		GameClearBGImageW = GameClearBGImage.GetWidth(); GameClearBGImageH = GameClearBGImage.GetHeight();

		// 배경 이미지 정보 가져오기 /////////////////////////////////////////////////
		back[0].Load(Assets::background_1().c_str());
		back[1].Load(Assets::background_2().c_str());
		backX = back[0].GetWidth(); backY = back[0].GetHeight();

		CImage tmp;
		tmp.Create(client.right, client.bottom, 32);
		HDC hdcTmp = tmp.GetDC();
		back[0].Draw(hdcTmp, 0, 0, client.right, client.bottom, 0, 0, backX, backY);
		tmp.ReleaseDC();
		backBitmap = tmp.Detach();
		///////////////////////////////////////////////////////////////////////////////

		// 플레이어 이미지 정보 가져오기 
		playerImage.Load(Assets::playerImage().c_str());
		playerImageX = playerImage.GetWidth();
		playerImageY = playerImage.GetHeight();
		// 플레이어 구조체 초기화
		player.size = 100;
		player.x = client.right / 2 - 50; player.y = client.bottom - 150; // 
		player.hp = 10;
		player.revive = false;
		player.reviveAnimation = 0;
		// 플레이어 속도 관련
		player_speed = 5.0f;
		speed_up_timer = 0;

		// 적 이미지 정보 가져오기
		enemyImage.Load(Assets::enemyImage().c_str());
		enemyImage2.Load(Assets::enemyImage2().c_str());
		enemyImage3.Load(Assets::enemyImage3().c_str());

		enemyImageX = enemyImage.GetWidth();
		enemyImageY = enemyImage.GetHeight();
		// 적 구조체 초기화
		enemy.size = 100;
		enemy.x = client.right / 2 - player.size; enemy.y = client.top + 30;
		enemy.speed = 3.0;
		float angle = (float)(rand() % 360) * PIE / 180.0;
		enemy.dx = cos(angle); enemy.dy = sin(angle);
		enemy.hp = 300;
		enemy.directionChangeCounter = 0;


		// 총알 이미지 정보 가져오기
		bulletImage.Load(Assets::bulletImage().c_str());
		bulletImageX = bulletImage.GetWidth(); bulletImageY = bulletImage.GetHeight();

		explosionImage[0].Load(Assets::Hit_1_0001().c_str());
		explosionImage[1].Load(Assets::Hit_1_0002().c_str());
		explosionImage[2].Load(Assets::Hit_1_0003().c_str());
		explosionImage[3].Load(Assets::Hit_1_0004().c_str());
		explosionImageX = explosionImage[0].GetWidth();  explosionImageY = explosionImage[0].GetHeight();

		// 총알 구조체 초기화
		for (int i = 0; i < MAX_BULLET; ++i)
		{
			bullet[i].size = 30;
			bullet[i].speed = 1.0;
			bullet[i].x = player.x + player.size / 2 - 10;
			bullet[i].y = player.y;
			bullet[i].active = false;
			bullet[i].explosion = false;

		}

		// 플레이어 폭발충돌 이미지 로드
		playerExplo[0].Load(Assets::Explosion_1_0001().c_str());
		playerExplo[1].Load(Assets::Explosion_1_0002().c_str());
		playerExplo[2].Load(Assets::Explosion_1_0003().c_str());
		playerExplo[3].Load(Assets::Explosion_1_0004().c_str());
		playerExW = playerExplo[0].GetWidth(), playerExH = playerExplo[0].GetWidth();

		// 장애물(빨간원) 이미지 정보 가져오기
		redcircleImage.Load(Assets::redcircle().c_str());
		redcircleImageW = redcircleImage.GetWidth(), redcircleImageH = redcircleImage.GetHeight();
		// 장애물(빨간원) 구조체 초기화
		for (int i = 0; i < MAX_REDCIRCLE; ++i)
		{
			redcircle[i].size = 30.0;
			redcircle[i].x = enemy.x + enemy.size / 2;
			redcircle[i].y = enemy.y + enemy.size;
			redcircle[i].speed = 3.0;
			redcircle[i].active = false;
		}

		// 운석 이미지 정보 가져오기
		meteorImage[0].Load(Assets::meteor1().c_str());
		meteorImage[1].Load(Assets::meteor2().c_str());
		meteorImage[2].Load(Assets::meteor3().c_str());
		meteorImageW = meteorImage[0].GetWidth(), meteorImageH = meteorImage[0].GetHeight();
		// 운석 구조체 초기화
		meteor.size = 150.0;
		meteor.y = 0.0;
		meteor.lineActive = false;
		meteor.meteorActive = false;
		meteor.animationCnt = 0;

		meteor.lineY = 0;
		meteor.delayCnt = 0;

		meteor.explosion = false;


		// 아이템 슬롯 이미지 로드
		ItemSlotImage.Load(Assets::Itemslot().c_str());
		ItemSlotImageW = ItemSlotImage.GetWidth(), ItemSlotImageH = ItemSlotImage.GetHeight();

		// 아이템 생성 전 이미지
		item_effectImage[0].Load(Assets::item_effect_0().c_str());
		item_effectImage[1].Load(Assets::item_effect_1().c_str());
		item_effectImage[2].Load(Assets::item_effect_2().c_str());
		item_effectImage[3].Load(Assets::item_effect_3().c_str());

		// 아이템 이미지 로드
		itemImages[ITEM_HEAL].Load(Assets::item_heal().c_str());
		itemImages[ITEM_SPEED].Load(Assets::item_speed().c_str());
		itemImages[ITEM_POSION].Load(Assets::item_posion().c_str());
		itemImages[ITEM_FOG].Load(Assets::item_fog().c_str());
		itemImages[ITEM_REBIRTH].Load(Assets::item_rebirth().c_str());

#define ITEM_TYPE_COUNT 5
		for (int i = 0; i < ITEM_TYPE_COUNT; ++i)
		{
			item_image_W[i] = itemImages[i].GetWidth();
			item_image_H[i] = itemImages[i].GetHeight();
		}

		reviveImage[0].Load(Assets::revive1().c_str());
		reviveImage[1].Load(Assets::revive2().c_str());
		reviveImage[2].Load(Assets::revive3().c_str());
		reviveImageW = reviveImage[0].GetWidth(), reviveImageH = reviveImage[0].GetHeight();



		// 안개 관련
		fogImage.Load(Assets::fogImage().c_str());
		fog_image_W = fogImage.GetWidth(), fog_image_H = fogImage.GetHeight();
		fog_active = FALSE;
		fog_timer = 0;

		// 게임시작 전 화면 //////////////////////////////////
		bGameStart = false;
		startBox = { client.right / 2 - 150, client.bottom / 2 - 10, client.right / 2 + 150, client.bottom / 2 + 50 };
		//////////////////////////////////////////////////////
		GameTime = 0.0;
		redcircleCounter = 0;
		meteorCounter = 0;
		bGamePause = false;
		bGameOver = false;
		bGameClear = false;

		IsGetItem = false;
		blinkCnt = 0;
		// 배경 초기화
		current_back = 0;
		background_changed = FALSE;

		Init_Items();

		break;
	}
	case WM_PAINT:
	{
		hdc = BeginPaint(hWnd, &ps);
		mdc = CreateCompatibleDC(hdc); // 더블버퍼링을 위한 메모리디시 생성
		hBitmap = CreateCompatibleBitmap(hdc, client.right, client.bottom); // 더블버퍼링을 위한 비트맵 생성
		HBITMAP oldBitmap = (HBITMAP)SelectObject(mdc, (HBITMAP)hBitmap);



		// 플레이어와 보스 이동범위 제한 표시 ( 경계 확인할려고 임의로 그려뒀습니다 )
		/*MoveToEx(mdc, 0, 200, NULL);
		LineTo(mdc, client.right, 200);*/
		if (bGameOver)
		{
			PlaySound(NULL, 0, 0);
			GameEndBGImage.Draw(mdc, 0, 0, client.right, client.bottom, 0, 0, GameEndBGImageW, GameEndBGImageH);
			if (bGameClear)
				GameClearBGImage.Draw(mdc, client.right / 2 - 150, client.bottom / 2 - 100, 300, 200, 0, 0, GameClearBGImageW, GameClearBGImageH);
			else
				GameOverImage.Draw(mdc, client.right / 2 - 150, client.bottom / 2 - 100, 300, 200, 0, 0, overImageW, overImageH);


			SetBkMode(mdc, TRANSPARENT);
			TextOut(mdc, client.right / 2 - 110, client.bottom / 2 + 50, L"Press Enter to Restart / Esc to Quit", lstrlen(L"Press Enter to Restart / Esc to Quit"));

			TCHAR timeBuf[64];
			swprintf_s(timeBuf, _countof(timeBuf), L"Your PlayTime: %.1f sec", GameTime);
			TextOutW(mdc, client.right / 2 - 70, client.bottom / 2 + 80, timeBuf, lstrlenW(timeBuf));

			wchar_t scoreBuf[64];
			swprintf_s(scoreBuf, L"Your Score: %d", score);
			TextOutW(mdc, client.right / 2 - 60, client.bottom / 2 + 110, scoreBuf, (int)wcslen(scoreBuf));
		}



		if (bGameStart == false && bGameOver == false) // 게임시작 전 화면
		{
			GameStartBGImage.Draw(mdc, 0, 0, client.right, client.bottom, 0, 0, GameStartBGImageW, GameStartBGImageH);

			SetBkMode(mdc, TRANSPARENT);
			TextImage.Draw(mdc, client.right / 2 - 150, client.bottom / 2 - 135, 300, 90, 0, 0, textW, textH);
			SetTextColor(mdc, RGB(255, 255, 255));

			//Rectangle(mdc, startBox.left, startBox.top, startBox.right, startBox.bottom);
			GameStartImage.Draw(mdc, client.right / 2 - 150, client.bottom / 2 - 80, 300, 200, 0, 0, startImageW, startImageH);

			TCHAR buf[50];
			swprintf_s(buf, L"Click 'START' TO GAME START", lstrlen(L"Click 'START' TO GAME START"));
			TextOut(mdc, client.right / 2 - 100, client.bottom / 2 - 100, buf, lstrlen(buf));

		}
		else if (bGameStart && bGameOver == false) // 게임 시작 후 화면
		{
			//////////////////////////////////////////////////////////////////////////////////
			// 배경 그리기
			int back_height = back[current_back].GetHeight();	// 현재 사용중인 이미지 높이 저장
			int back_width = back[current_back].GetWidth();		// 현재 사용중인 이미지 너비 저장
			int scrollY = (int)back_scrollY % back_height; // 스크롤 위치 계산

			// scrollY 음수로 바꾸어서 아래에서 위로 이동하게 바꾼다!
			if (scrollY > 0) { scrollY -= back_height; }

			// 반복문으로 수정 -> 화면 전체 채우도록 이미지 그림
			for (int y = -back_height; y < client.bottom + back_height; y += back_height)
			{
				back[current_back].Draw(mdc, 0, y + scrollY, client.right, back_height, 0, 0, back_width, back_height);
			}
			/////////////////////////////////////////////////////////////////////////////////

			// 아이템슬롯 왼쪽 아래
			ItemSlotImage.Draw(mdc, 0, client.bottom - 100, 100, 100, 0, 0, ItemSlotImageW, ItemSlotImageH);
			if (has_rebirth)
				itemImages[ITEM_REBIRTH].Draw(mdc, 0, client.bottom - 100, 100, 100, 0, 0, item_image_W[ITEM_REBIRTH], item_image_H[ITEM_REBIRTH]);

			// 보스 출력
			if (enemy.hp > 200 && enemy.hp <= 300)
				enemyImage.Draw(mdc, enemy.x, enemy.y, enemy.size, enemy.size, 0, 0, enemyImageX, enemyImageY);
			else if (enemy.hp > 100 && enemy.hp <= 200)
				enemyImage2.Draw(mdc, enemy.x, enemy.y, enemy.size, enemy.size, 0, 0, enemyImageX, enemyImageY);
			else
				enemyImage3.Draw(mdc, enemy.x, enemy.y, enemy.size, enemy.size, 0, 0, enemyImageX, enemyImageY);
			// 보스 체력바 출력
			TCHAR buf[50];
			swprintf_s(buf, _countof(buf), L"enemy hp : %d", enemy.hp);
			TextOut(mdc, enemy.x + enemy.size, enemy.y + enemy.size, buf, lstrlen(buf));

			const int hpbarheight = 20;
			RECT hpBarBackGround = { enemy.x, enemy.y - hpbarheight, enemy.x + enemy.size, enemy.y - hpbarheight / 2 }; // 체력바 배경
			HBRUSH hpbackBrush = CreateSolidBrush(RGB(100, 100, 100));
			FillRect(mdc, &hpBarBackGround, hpbackBrush);
			DeleteObject(hpbackBrush);

			RECT hpBar = { enemy.x, enemy.y - hpbarheight, enemy.x + enemy.hp / 3 , enemy.y - hpbarheight / 2 }; // enemy.hp == 300  // enemy.size == 100
			HBRUSH hpbarBrush = CreateSolidBrush(RGB(255, 0, 0));
			FillRect(mdc, &hpBar, hpbarBrush);
			DeleteObject(hpbarBrush);
			///////////////////////////////////////////




			// 플레이어 출력
			if (IsGetItem == false) // 플레이어 아이템 먹을시 깜빡임
				playerImage.Draw(mdc, player.x, player.y, player.size, player.size, 0, 0, playerImageX, playerImageY);
			else if (blinkCnt % 2 == 0)
				playerImage.Draw(mdc, player.x, player.y, player.size, player.size, 0, 0, playerImageX, playerImageY);

			// 플레이어 체력바 출력
			TCHAR buff[50];
			swprintf_s(buff, _countof(buff), L"player hp : %d", player.hp);
			TextOut(mdc, player.x + player.size, player.y + player.size, buff, lstrlen(buff));

			const int hpmarin = 5;
			RECT playerhpback = { player.x, player.y + player.size + hpmarin, player.x + player.size, player.y + player.size + hpmarin * 2 }; // player.size == 100
			hpbackBrush = CreateSolidBrush(RGB(100, 100, 100));
			FillRect(mdc, &playerhpback, hpbackBrush);
			DeleteObject(hpbackBrush);
			RECT playerhp = { player.x, player.y + player.size + hpmarin, player.x + player.hp * 10 , player.y + player.size + hpmarin * 2 }; // player.hp == 10
			hpbarBrush = CreateSolidBrush(RGB(255, 0, 0));
			FillRect(mdc, &playerhp, hpbarBrush);
			DeleteObject(hpbarBrush);

			/////////////////////////////////////////////

			// 총알 및 폭발 출력
			for (int i = 0; i < MAX_BULLET; ++i)
			{
				if (bullet[i].active)
					bulletImage.Draw(mdc, bullet[i].x, bullet[i].y, bullet[i].size, bullet[i].size, 0, 0, bulletImageX, bulletImageY);

				if (bullet[i].explosion)
					explosionImage[bullet[i].explosionCnt].Draw(mdc, bullet[i].exploX, bullet[i].exploY, 50, 50, 0, 0, explosionImageX, explosionImageY);
			}

			// 보스 장애물 (빨간원) 출력
			for (int i = 0; i < MAX_REDCIRCLE; ++i)
			{
				if (redcircle[i].active)
					redcircleImage.Draw(mdc, redcircle[i].x, redcircle[i].y, redcircle[i].size, redcircle[i].size, 0, 0, redcircleImageW, redcircleImageH);

				// 폭발 출력
				if (redcircle[i].explosion)
					playerExplo[redcircle[i].explosionCnt].Draw(mdc, redcircle[i].exploX, redcircle[i].exploY, 75, 75, 0, 0, playerExW, playerExH);
			}

			// 빨간 선 및 운석 출력	
			if (meteor.lineActive)
			{
				HPEN hPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
				HPEN hOld = (HPEN)SelectObject(mdc, hPen);
				MoveToEx(mdc, meteor.x + (meteor.size / 2), 0, NULL);
				LineTo(mdc, meteor.x + (meteor.size / 2), meteor.lineY);
				SelectObject(mdc, hOld);
				DeleteObject(hPen);

				if (meteor.meteorActive)
				{
					//RECT rMeteor = { meteor.x + 20,meteor.y + 20, meteor.x + meteor.size - 20, meteor.y + meteor.size - 20 };
					//Rectangle(mdc, rMeteor.left, rMeteor.top, rMeteor.right, rMeteor.bottom);
					meteorImage[meteor.animationCnt].Draw(mdc, meteor.x, meteor.y, meteor.size, meteor.size, 0, 0, meteorImageW, meteorImageH);
				}
			}

			// 운석 충돌 폭발
			if (meteor.explosion)
				playerExplo[meteor.explosionCnt].Draw(mdc, player.x, player.y, 100, 100, 0, 0, playerExW, playerExH);

			// 부활 이펙트
			if (player.revive)
				reviveImage[player.reviveAnimation].Draw(mdc, player.x, player.y, 100, 100, 0, 0, reviveImageW, reviveImageH);

			// 아이템 생성 전 이펙트
			for (int i = 0; i < MAX_ITEM_EFFECT; ++i)
			{
				if (itemEffects[i].active)
				{
					int frame = itemEffects[i].animation_count % 4;
					int newSize = 100; // 크기 고정

					item_effectImage[frame].Draw(mdc, itemEffects[i].x, itemEffects[i].y, newSize, newSize,
						0, 0, item_effectImage[frame].GetWidth(), item_effectImage[frame].GetHeight());
				}
			}


			// 아이템 생성
			for (int i = 0; i < MAX_ITEM; ++i)
			{
				if (!items[i].active)
				{
					continue;
				}

				float x = items[i].x;
				float y = items[i].y;
				float size = items[i].size - 10;

				itemImages[items[i].type].Draw(mdc, x, y, size, size, 0, 0, item_image_W[items[i].type], item_image_H[items[i].type]);
			}


			// 안개 활성화 시
			if (fog_alpha > 0)
			{
				// 화면에 안개 출력
				HDC fogDC = CreateCompatibleDC(mdc);
				HBITMAP fogBmp = fogImage.Detach();
				SelectObject(fogDC, fogBmp);

				BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)fog_alpha, AC_SRC_ALPHA };
				AlphaBlend(mdc, 0, 0, client.right, client.bottom,
					fogDC, 0, 0, fog_image_W, fog_image_H, bf);

				fogImage.Attach(fogBmp);
				DeleteDC(fogDC);
			}

			// 화면 바래면서 변화
			if (fade_alpha > 0 && !background_changed)
			{
				HBRUSH hFade = CreateSolidBrush(RGB(255, 255, 240));	// 색 바꾸면 변함!!
				HDC fadeDC = CreateCompatibleDC(mdc);
				HBITMAP fadeBitmap = CreateCompatibleBitmap(mdc, client.right, client.bottom);
				SelectObject(fadeDC, fadeBitmap);
				FillRect(fadeDC, &client, hFade);

				BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)fade_alpha,0 };
				AlphaBlend(mdc, 0, 0, client.right, client.bottom, fadeDC, 0, 0, client.right, client.bottom, bf);
				DeleteObject(hFade);
				DeleteObject(fadeBitmap);
				DeleteDC(fadeDC);
			}

			int icon_size = 55;		// 아이콘 크기
			int spacing = 6;		// 아이콘 간 간격
			int i_x = client.right - icon_size - spacing;
			int i_y = 10;

			// 구름 먹고 보스있는 위치 가려지기
			if (is_fog_active)
			{
				int cloudY = -70;
				int cloudHeight = 400;          // 충분히 넓은 높이로 라인 전체를 가림
				int cloudWidth = client.right + 100;  // 화면 전체 너비

				itemImages[ITEM_FOG].Draw(mdc, 0, cloudY, cloudWidth, cloudHeight, 0, 0, item_image_W[ITEM_FOG], item_image_H[ITEM_FOG]);
			}


			if (is_fog_active)
			{
				Draw_Glow_Effect(mdc, i_x, i_y, icon_size);
				itemImages[ITEM_FOG].Draw(mdc, i_x, i_y, icon_size, icon_size, 0, 0,
					item_image_W[ITEM_FOG], item_image_H[ITEM_FOG]);
				i_x -= (icon_size + spacing);
			}
			if (is_speed_active)
			{
				Draw_Glow_Effect(mdc, i_x, i_y, icon_size);

				itemImages[ITEM_SPEED].Draw(mdc, i_x, i_y, icon_size, icon_size, 0, 0,
					item_image_W[ITEM_SPEED], item_image_H[ITEM_SPEED]);
				i_x -= (icon_size + spacing);
			}

			if (bGamePause)
			{
				HPEN hBlack = CreatePen(PS_SOLID, 2, BLACK_PEN);
				HPEN oldpen = (HPEN)SelectObject(mdc, hBlack);
				int rSize = 50;
				RECT rCenter = { client.right / 2 - rSize * 2,client.bottom / 2 - rSize,
					client.right / 2 + rSize * 2,client.bottom / 2 + rSize };
				Rectangle(mdc, rCenter.left, rCenter.top, rCenter.right, rCenter.bottom);
				SetBkMode(mdc, TRANSPARENT); // 배경 투명하게 설정
				DrawText(mdc, L"GamePause", lstrlen(L"GamePause"), &rCenter, DT_VCENTER | DT_SINGLELINE | DT_CENTER);

				SelectObject(mdc, oldpen);
				DeleteObject(hBlack);
			}
		}

		ReportFps(bGameStart && !bGameOver && !bGamePause);

		BitBlt(hdc, 0, 0, client.right, client.bottom, mdc, 0, 0, SRCCOPY);
		SelectObject(mdc, oldBitmap);
		DeleteObject(hBitmap);
		DeleteDC(mdc);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		POINT mouse = { LOWORD(lParam), HIWORD(lParam) };
		RECT click = { startBox.left, startBox.top,  startBox.right, startBox.bottom };
		if (PtInRect(&click, mouse)) // 게임시작 전 화면에서 클릭 !!
		{
			input.Reset();
			bGameStart = true;

			PlaySound(NULL, NULL, 0);
			// 게임시작 시 배경음악 재생
			PlaySound(Assets::background_bgm().c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);

			SetTimer(hWnd, 1, 17, NULL);   // 플레이어 이동
			SetTimer(hWnd, 2, 17, NULL);   // 보스, 총알, 충돌 등
			SetTimer(hWnd, 3, 100, NULL);  // 아이템 시간과 효과는 그대로
		}
		break;
	}
	case WM_KEYDOWN:
	{
		if (bGameOver) // 게임 종료일때
		{
			if (wParam == VK_RETURN)  // Enter 눌렀을 때 재시작
			{
				// 1) 상태 플래그 리셋
				input.Reset();
				bGameOver = false;
				bGameStart = true;
				bGamePause = false;
				bGameClear = false;

				// 2) 타이머 재설정
				SetTimer(hWnd, 1, 17, NULL);
				SetTimer(hWnd, 2, 17, NULL);
				SetTimer(hWnd, 3, 100, NULL);

				// 3) 주요 카운터/플래그 초기화
				GameTime = 0.0f;
				redcircleCounter = 0;
				meteorCounter = 0;
				item_counter = 0.0f;
				speed_up_timer = 0;
				fog_active = FALSE;
				fog_timer = 0;
				has_rebirth = FALSE;
				IsGetItem = false;
				blinkCnt = 0;
				reviveCnt = 0;
				score = 0;
				GameTime = 0.0f;

				// 4) 플레이어 초기화
				player.hp = 10;
				player.x = client.right / 2 - player.size / 2;
				player.y = client.bottom - 150;
				player.revive = false;
				player.reviveAnimation = 0;

				// 5) 보스 초기화
				enemy.hp = 300;
				enemy.x = client.right / 2 - player.size;
				enemy.y = client.top + 30;
				{
					float angle = (float)(rand() % 360) * PIE / 180.0f;
					enemy.dx = cosf(angle);
					enemy.dy = sinf(angle);
				}
				enemy.directionChangeCounter = 0;

				// 6) 총알 초기화
				bulletCnt = 0;
				for (int i = 0; i < MAX_BULLET; ++i)
				{
					bullet[i].active = false;
					bullet[i].explosion = false;
					bullet[i].explosionCnt = 0;
				}

				// 7) 빨간원(Obstacle) 초기화
				for (int i = 0; i < MAX_REDCIRCLE; ++i)
				{
					redcircle[i].active = false;
					redcircle[i].explosion = false;
					redcircle[i].explosionCnt = 0;
				}

				// 8) 운석 장애물 초기화
				meteor.lineActive = false;
				meteor.meteorActive = false;
				meteor.lineY = 0;
				meteor.y = 0;
				meteor.delayCnt = 0;
				meteor.animationCnt = 0;
				meteor.explosion = false;
				meteor.explosionCnt = 0;

				// 9) 아이템 초기화
				for (int i = 0; i < MAX_ITEM; ++i)
					items[i].active = false;

				// 10) 화면 즉시 갱신

				background_changed = FALSE;
				start_fade = FALSE;
				end_fade = FALSE;
				fade_alpha = 0;
				current_back = 0;
			}
			else if (wParam == VK_ESCAPE)  // Esc 눌러서 종료
			{
				DestroyWindow(hWnd);
			}
			break;
		}

		if (bGameStart && !bGamePause && !bGameClear)
		{
			input.OnKeyDown(static_cast<unsigned int>(wParam));
		}

		switch (wParam)
		{
		case VK_F1:  // 게임 퍼즈
			if (!bGameStart || bGameClear) break;
			input.Reset();
			if (bGamePause == false)
			{
				bGamePause = true;
				KillTimer(hWnd, 1);
				KillTimer(hWnd, 2);
				KillTimer(hWnd, 3);
			}
			else
			{
				bGamePause = false;
				SetTimer(hWnd, 1, 17, NULL); // 방향키 전용 타이머
				SetTimer(hWnd, 2, 17, NULL); // 다른 객체 이동 및 충돌처리
				SetTimer(hWnd, 3, 100, NULL); // 각종 시간주기 계산용 ( 장애물,아이템 스폰타이밍 등 )
			}
			break;

		} // switch
		break;
	}
	case WM_KEYUP:
		input.OnKeyUp(static_cast<unsigned int>(wParam));
		break;
	case WM_KILLFOCUS:
		input.Reset();
		break;
	case WM_TIMER:
	{
		if (!bGameStart || bGamePause || bGameOver || bGameClear)
		{
			input.Reset();
			break;
		}
		switch (wParam)
		{
		case 1: { // 입력 명령을 받아 플레이어 이동과 발사 처리
			const InputCommand command = input.ConsumeCommand();
			//int move = 5; // 5만큼 플레이어 이동 ( 플레이어 이동 예외처리완료 )
			// 속도 조절 아이템을 위해서 move 가 아니라 전역으로 player_speed 변수를 설정하였습니다!
			float move = player_speed;

			if (command.up && player.y > 150)
				player.y -= move;

			if (command.down && player.y + player.size < client.bottom)
				player.y += move;

			if (command.right && player.x + player.size < client.right)
				player.x += move;

			if (command.left && player.x > client.left)
				player.x -= move;
			// 총알 위치 최신화 => 플레이어 위치에 의존
			for (int i = 0; i < MAX_BULLET; ++i)
			{
				if (bullet[i].active == false)
				{
					bullet[i].x = player.x + player.size / 2 - 15;
					bullet[i].y = player.y;
				}
			}
			if (command.firePressed)
			{
				bullet[bulletCnt].active = true;
				bulletCnt = (bulletCnt + 1) % MAX_BULLET;
				mciSendString(L"play shotSound from 0", NULL, 0, NULL);
			}
			break;
		}
		case 2: { // 나머지 이동 및 충돌처리 타이머

			MoveEnemy(); // 적 
			ShotBullet(); // 총알 이동
			MoveRedCircle(); // 장애물(빨간원) 이동
			MoveMeteor();
			int gameend = CheckEnemyBulletCollision(); // 총알과 적 충돌처리
			int gameover = CheckRedcirclePlayerCollision(); // 장애물(빨간원) 플레이어 충돌처리
			int gameover2 = CheckMeteorPlayerCollision();
			int gameover3 = Check_Item_Player_Collision();
			if (gameover == 1 || gameover2 == 1 || gameover3 == 1 || gameend == 1) // 게임종료 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			{
				if (gameend == 1)
					bGameClear = true; // 보스 쓰러뜨리며 게임 종료
				input.Reset();
				bGameOver = true;
				KillTimer(hWnd, 1);
				KillTimer(hWnd, 2);
				KillTimer(hWnd, 3);

				PlaySound(NULL, 0, 0);
				mciSendString(L"stop shotSound", NULL, 0, NULL);
				mciSendString(L"stop hurtSound", NULL, 0, NULL);
				mciSendString(L"stop healSound", NULL, 0, NULL);
				mciSendString(L"stop posionSound", NULL, 0, NULL);
				mciSendString(L"stop speedSound", NULL, 0, NULL);
				mciSendString(L"stop reviveSound", NULL, 0, NULL);
				mciSendString(L"stop hitSound", NULL, 0, NULL);
				mciSendString(L"stop changeSound", NULL, 0, NULL);
				mciSendString(L"stop meteorSound", NULL, 0, NULL);
				mciSendString(L"stop fogSound", NULL, 0, NULL);

				if (gameend == 1)
				{
					mciSendString(L"play winSound from 0", NULL, 0, NULL);
				}
				else
				{
					mciSendString(L"play loseSound from 0", NULL, 0, NULL);
				}
				break;
			}

			Update_Items();	// 아이템 이동과 제거
			Check_Item_Player_Collision();

			Change_Background_Fade(hWnd);
			// 위에서 아래로
			back_scrollY += back_scroll_speed * OBJECT_MOVE_RATIO;
			if (back_scrollY >= back[current_back].GetHeight())
			{
				back_scrollY -= back[current_back].GetHeight();
			}
			break;
		}
		case 3: // 각종 시간주기 계산용
		{
			GameTime += 0.1; // 게임시간

			// 빨간원 장애물
			redcircleCounter++;
			if (redcircleCounter == 10)
			{
				SpawnRedcircle();
				redcircleCounter = 0;
			}

			// 운석 장애물
			if (enemy.hp <= 200)
			{
				if (meteor.lineActive == false)
				{
					meteorCounter++;
					if (meteorCounter >= 10) // 1초
					{
						meteor.lineActive = true;
						meteor.x = 10 + (rand() % (client.right - 100));
						meteorCounter = 0;
					}
				}
			}

			if (meteor.lineActive) // 빨간선 그려지고 다 그려지면 운석생성이동
			{
				meteor.lineY += 30;

				if (meteor.lineY >= client.bottom)
				{
					meteor.meteorActive = true;
					mciSendString(L"play meteorSound from 0", NULL, 0, NULL);
				}
			}

			// 아이템 생성 전 이펙트
			for (int i = 0; i < MAX_ITEM_EFFECT; ++i)
			{
				if (itemEffects[i].active)
				{
					itemEffects[i].animation_count++;
					if (itemEffects[i].animation_count >= itemEffects[i].duration)
					{
						// 이펙트 끝나면 아이템 활성화
						itemEffects[i].active = false;
						items[i].active = TRUE;
					}
				}
			}


			// 아이템 생성
			item_counter++;
			if (item_counter == 60)	// 6초
			{
				Spawn_Item();
				item_counter = 0;
			}

			if (speed_up_timer > 0)
			{
				speed_up_timer--;
				if (speed_up_timer == 0)
				{
					player_speed = 5.0f;	// 원래 속도로 복원
					is_speed_active = FALSE;

					mciSendString(L"stop speedSound", NULL, 0, NULL);
				}
			}

			if (fog_fade_in)
			{
				fog_alpha += 10;
				if (fog_alpha >= 255)
				{
					fog_alpha = 255;
					fog_fade_in = FALSE;
				}
			}
			else if (fog_timer > 0)
			{
				fog_timer--;
				if (fog_timer == 0)
				{
					fog_fade_out = TRUE;

					mciSendString(L"stop fogSound", NULL, 0, NULL);
				}
			}
			else if (fog_fade_out)
			{
				fog_alpha -= 10;
				if (fog_alpha <= 0)
				{
					fog_alpha = 0;
					fog_fade_out = FALSE;
					is_fog_active = FALSE;
				}
			}

			playerExplosion(); // 플레이어 충돌폭발 ( 좀 느린 애니메이션을 위해 여기에다가 넣었습니다 )

			// 아이템 먹을시 플레이어 깜빡거림
			if (IsGetItem)
			{
				blinkCnt++;
				if (blinkCnt == 30) // 3초간 깜빡거림
				{
					IsGetItem = false;
					blinkCnt = 0;
				}
			}

			if (player.revive)
			{
				reviveCnt++;
				if (reviveCnt == 2)
				{
					player.reviveAnimation++;
					reviveCnt = 0;
				}
				if (player.reviveAnimation == 3)
				{
					player.revive = false;
					player.reviveAnimation = 0;
					reviveCnt = 0;
				}
			}

			break;
		}
		}
		break;
	}
	case WM_DESTROY:
		PlaySound(NULL, 0, 0);
		mciSendString(L"close shotSound", NULL, 0, NULL);
		mciSendString(L"close hurtSound", NULL, 0, NULL);
		mciSendString(L"close winSound", NULL, 0, NULL);
		mciSendString(L"close healSound", NULL, 0, NULL);
		mciSendString(L"close posionSound", NULL, 0, NULL);
		mciSendString(L"close speedSound", NULL, 0, NULL);
		mciSendString(L"close reviveSound", NULL, 0, NULL);
		mciSendString(L"close hitSound", NULL, 0, NULL);
		mciSendString(L"close changeSound", NULL, 0, NULL);
		mciSendString(L"close meteorSound", NULL, 0, NULL);
		mciSendString(L"close fogSound", NULL, 0, NULL);
		mciSendString(L"close loseSound", NULL, 0, NULL);

		DeleteObject(backBitmap);
		back[0].Destroy();
		back[1].Destroy();
		playerImage.Destroy();
		enemyImage.Destroy();
		enemyImage2.Destroy();
		enemyImage3.Destroy();
		bulletImage.Destroy();
		GameStartBGImage.Destroy();
		GameStartImage.Destroy();
		GameEndBGImage.Destroy();
		GameOverImage.Destroy();
		TextImage.Destroy();
		fogImage.Destroy();
		for (int i = 0; i < 4; ++i)
			explosionImage[i].Destroy();
		for (int i = 0; i < 4; ++i)
			playerExplo[i].Destroy();
		for (int i = 0; i < 5; ++i)
			itemImages[i].Destroy();
		for (int i = 0; i < 3; ++i)
			meteorImage[i].Destroy();
		for (int i = 0; i < 4; ++i)
		{
			item_effectImage[i].Destroy();
		}
		redcircleImage.Destroy();
		ItemSlotImage.Destroy();
		KillTimer(hWnd, 1);
		KillTimer(hWnd, 2);
		KillTimer(hWnd, 3);
		PostQuitMessage(0);
		return 0;
	}
	return (DefWindowProc(hWnd, iMessage, wParam, lParam));
}