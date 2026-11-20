#include "Client/ClientNetwork.h"

#include "Assets.h"
#include <tchar.h>
#include <time.h>
#include <stdbool.h>
#include <windowsx.h>	// AlphaBlend() 함수 이용
#include <mmsystem.h>	

#include "Client/Input.h"
#include "Client/Render.h"
#include "Shared/GameSimulation.h"


#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")


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

	// 11.11
	ClientNetwork network;
	SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&network));
	if (network.Connect("127.0.0.1", 9000, hWnd))
	{
		OutputDebugStringW(L"[TCP 클라이언트] TCP 연결 성공\n");
	}
	else
	{
		OutputDebugStringW(L"[TCP 클라이언트] TCP 연결 실패\n");
	}

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


CImage back[2]; // 배경 이미지
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
CImage EmptyPlayerSlotImage;		 // 플레이어 접속 전 이미지.
CImage ReadyPlayerSlotImage;		 // 플레이어 접속 후 이미지
int startImageW, startImageH;

CImage GameOverImage;
int overImageW, overImageH;

CImage TextImage;
int textW, textH;

CImage buttonImage;
int buttonW, buttonH;


CImage playerImage; // 플레이어 이미지
int playerImageX, playerImageY; // 이미지 사이즈


CImage enemyImage; // 적 이미지
CImage enemyImage2;
CImage enemyImage3;
int enemyImageX, enemyImageY; // 적 이미지 사이즈


CImage bulletImage; // 총알 이미지
int bulletImageX, bulletImageY; // 총알 이미지 사이즈


CImage explosionImage[4];
int explosionImageX, explosionImageY;

CImage playerExplo[4];
int playerExW, playerExH;


CImage redcircleImage;
int redcircleImageW, redcircleImageH;


CImage meteorImage[3];
int meteorImageW, meteorImageH;


// 아이템들 종류

CImage itemImages[5];
int item_image_W[5], item_image_H[5];

// 아이템 슬롯 이미지 정보
CImage ItemSlotImage;
int ItemSlotImageW, ItemSlotImageH;




// 아이템 등장시 이펙트
CImage item_effectImage[4];


CImage reviveImage[3];
int reviveImageW, reviveImageH;

CImage fogImage;
int fog_image_W, fog_image_H;

// 보스 체력에 따른 배경변화!
// 일정 체력에 도달 시 바로 배경이 변하면 부자연스러워서
// AlphaBlend() 함수 이용하여서 자연스럽게 구현해보았습니다!
bool background_changed = FALSE;
bool start_fade = FALSE;
bool end_fade = FALSE;
int fade_alpha = 0;

void Change_Background_Fade(int enemyHp)
{
	if (!background_changed && !start_fade && enemyHp <= 200)	// 체력 도달
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

void PlayGameEvent(GameEvent event)
{
	const wchar_t* command = nullptr;
	switch (event)
	{
	case GameEvent::BossHit: command = L"play hitSound from 0"; break;
	case GameEvent::PlayerHit: command = L"play hurtSound from 0"; break;
	case GameEvent::Revived: command = L"play reviveSound from 0"; break;
	case GameEvent::Healed: command = L"play healSound from 0"; break;
	case GameEvent::SpeedStarted: command = L"play speedSound from 0"; break;
	case GameEvent::Poisoned: command = L"play posionSound from 0"; break;
	case GameEvent::FogStarted: command = L"play fogSound from 0"; break;
	case GameEvent::MeteorStarted: command = L"play meteorSound from 0"; break;
	case GameEvent::SpeedEnded: command = L"stop speedSound"; break;
	case GameEvent::FogEnded: command = L"stop fogSound"; break;
	}
	if (command)
		mciSendString(command, NULL, 0, NULL);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc, mdc;

	static HBITMAP hBitmap;

	static Input input;
	static Render render;

    static int myPlayerNumber = 0; // 서버 배정 플레이어 번호
    static int hostPlayerNumber = 0; // 현재 방장 번호
    static bool remoteVisible = false; // 상대 기체 좌표 수신 여부
    static bool occupied[2]{}; // 로비 슬롯 접속 여부
	static bool bGameStart; // 게임시작을 위한 변수 => START BOX 누를시 시작
	static RECT startBox; // START BOX 
	static bool bGamePause; // 게임 일시정지
	static bool bGameOver; // 게임종료
	static bool bGameClear; // 보스 쓰러뜨리며 게임종료

	static GameState gameState{};

	switch (iMessage) {
	case WM_ERASEBKGND:
		// 배경 지워서 계속 깜빡임
		// 그래서 직접 그리게 해서 깜빡임 방지하게 하기!
		return 1;
	case WM_CREATE: {
		srand((unsigned)time(NULL));
		GetClientRect(hWnd, &client);
		ResetGame(gameState, {client.left, client.top, client.right, client.bottom});

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
		EmptyPlayerSlotImage.Load(Assets::playerNone().c_str());
		ReadyPlayerSlotImage.Load(Assets::playerReady().c_str());
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
		
		///////////////////////////////////////////////////////////////////////////////

		// 플레이어 이미지 정보 가져오기 
		playerImage.Load(Assets::playerImage().c_str());
		playerImageX = playerImage.GetWidth();
		playerImageY = playerImage.GetHeight();

		// 적 이미지 정보 가져오기
		enemyImage.Load(Assets::enemyImage().c_str());
		enemyImage2.Load(Assets::enemyImage2().c_str());
		enemyImage3.Load(Assets::enemyImage3().c_str());

		enemyImageX = enemyImage.GetWidth();
		enemyImageY = enemyImage.GetHeight();

		// 총알 이미지 정보 가져오기
		bulletImage.Load(Assets::bulletImage().c_str());
		bulletImageX = bulletImage.GetWidth(); bulletImageY = bulletImage.GetHeight();

		explosionImage[0].Load(Assets::Hit_1_0001().c_str());
		explosionImage[1].Load(Assets::Hit_1_0002().c_str());
		explosionImage[2].Load(Assets::Hit_1_0003().c_str());
		explosionImage[3].Load(Assets::Hit_1_0004().c_str());
		explosionImageX = explosionImage[0].GetWidth();  explosionImageY = explosionImage[0].GetHeight();


		// 플레이어 폭발충돌 이미지 로드
		playerExplo[0].Load(Assets::Explosion_1_0001().c_str());
		playerExplo[1].Load(Assets::Explosion_1_0002().c_str());
		playerExplo[2].Load(Assets::Explosion_1_0003().c_str());
		playerExplo[3].Load(Assets::Explosion_1_0004().c_str());
		playerExW = playerExplo[0].GetWidth(), playerExH = playerExplo[0].GetWidth();

		// 장애물(빨간원) 이미지 정보 가져오기
		redcircleImage.Load(Assets::redcircle().c_str());
		redcircleImageW = redcircleImage.GetWidth(), redcircleImageH = redcircleImage.GetHeight();

		// 운석 이미지 정보 가져오기
		meteorImage[0].Load(Assets::meteor1().c_str());
		meteorImage[1].Load(Assets::meteor2().c_str());
		meteorImage[2].Load(Assets::meteor3().c_str());
		meteorImageW = meteorImage[0].GetWidth(), meteorImageH = meteorImage[0].GetHeight();

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

		// 게임시작 전 화면 //////////////////////////////////
		bGameStart = false;
		startBox = { client.right / 2 - 150, client.bottom / 2 - 10, client.right / 2 + 150, client.bottom / 2 + 50 };
		//////////////////////////////////////////////////////
		bGamePause = false;
		bGameOver = false;
		bGameClear = false;

		// 배경 초기화
		current_back = 0;
		background_changed = FALSE;


		break;
	}
	case WM_PAINT:
	{
		const PlayerState& player = gameState.players[0];
		const Enemy& enemy = gameState.enemy;
		const auto& bullet = gameState.bullets;
		const auto& redcircle = gameState.redCircles;
		const Meteor& meteor = gameState.meteor;
		const auto& items = gameState.items;
		const auto& itemEffects = gameState.itemEffects;

		hdc = BeginPaint(hWnd, &ps);
		mdc = CreateCompatibleDC(hdc); // 더블버퍼링을 위한 메모리디시 생성
		hBitmap = CreateCompatibleBitmap(hdc, client.right, client.bottom); // 더블버퍼링을 위한 비트맵 생성
		HBITMAP oldBitmap = (HBITMAP)SelectObject(mdc, (HBITMAP)hBitmap);


		// 플레이어와 보스 이동범위 제한 표시 ( 경계 확인할려고 임의로 그려뒀습니다 )
		/*MoveToEx(mdc, 0, 200, NULL);
		LineTo(mdc, client.right, 200);*/
		if (bGameOver)
		{
			CImage& resultImage = bGameClear ? GameClearBGImage : GameOverImage;

			render.DrawResultScreen(mdc, client.right, client.bottom,
				GameEndBGImage, resultImage, gameState.playTime, player.score);
		}


		if (bGameStart == false && bGameOver == false) // 게임 시작 전 화면
		{
			render.DrawStartScreen(mdc, client.right, client.bottom,
				GameStartBGImage, TextImage, GameStartImage, EmptyPlayerSlotImage,
                ReadyPlayerSlotImage, occupied, myPlayerNumber, hostPlayerNumber);
		}
		else if (bGameStart && bGameOver == false) // 게임 시작 후 화면
		{
			//////////////////////////////////////////////////////////////////////////////////
			// 배경 그리기는 Render에 위임합니다.
			render.DrawBackground(mdc, back[current_back],
				client.right, client.bottom, back_scrollY);
			/////////////////////////////////////////////////////////////////////////////////

			// 아이템 슬롯 왼쪽 아래
			render.DrawSprite(mdc, ItemSlotImage,
				0, client.bottom - 100, 100, 100, ItemSlotImageW, ItemSlotImageH);

			if (player.hasRebirth)
			{
				render.DrawSprite(mdc, itemImages[ITEM_REBIRTH],
					0, client.bottom - 100, 100, 100,
					item_image_W[ITEM_REBIRTH], item_image_H[ITEM_REBIRTH]);
			}

			// 보스 출력
			// 보스의 체력에 따라 표시할 이미지를 선택합니다.
			CImage* bossImage = &enemyImage3;

			if (enemy.hp > 200 && enemy.hp <= 300)
				bossImage = &enemyImage;
			else if (enemy.hp > 100 && enemy.hp <= 200)
				bossImage = &enemyImage2;

			render.DrawEnemy(mdc, *bossImage, enemyImageX, enemyImageY,
				enemy.x, enemy.y, enemy.size, enemy.hp);
			///////////////////////////////////////////


			// 플레이어 출력
			// 아이템을 먹은 동안에는 짝수 프레임에서만 캐릭터를 표시합니다.
			const bool showPlayerSprite = !player.blinking || player.blinkTicks % 2 == 0;

			render.DrawPlayer(mdc, playerImage,
				player.x, player.y, player.size, player.hp, showPlayerSprite);
            if (remoteVisible)
            {
                const PlayerState& remote = gameState.players[1];
                render.DrawSprite(mdc, playerImage, remote.x, remote.y,
                    remote.size, remote.size, playerImageX, playerImageY);
                wchar_t label[24]{};
                swprintf_s(label, L"PLAYER %d", myPlayerNumber == 1 ? 2 : 1);
                TextOutW(mdc, static_cast<int>(remote.x), static_cast<int>(remote.y) - 20,
                    label, lstrlenW(label));
            }

			/////////////////////////////////////////////

			// 총알 및 폭발 출력
			for (int i = 0; i < MAX_BULLET; ++i)
			{
				if (bullet[i].active)
				{
					render.DrawSprite(mdc, bulletImage,
						bullet[i].x, bullet[i].y,
						bullet[i].size, bullet[i].size,
						bulletImageX, bulletImageY);
				}

				if (bullet[i].explosion)
				{
					render.DrawSprite(mdc, explosionImage[bullet[i].explosionCnt],
						bullet[i].exploX, bullet[i].exploY,
						50, 50, explosionImageX, explosionImageY);
				}
			}

			// 보스 장애물과 충돌 폭발
			for (int i = 0; i < MAX_REDCIRCLE; ++i)
			{
				const auto& circle = redcircle[i];

				if (circle.active)
				{
					render.DrawSprite(mdc, redcircleImage,
						circle.x, circle.y, circle.size, circle.size,
						redcircleImageW, redcircleImageH);
				}

				if (circle.explosion)
				{
					render.DrawSprite(mdc, playerExplo[circle.explosionCnt],
						circle.exploX, circle.exploY, 75, 75,
						playerExW, playerExH);
				}
			}

			// 경고선을 먼저 그리고 그 위에 운석을 그립니다.
			if (meteor.lineActive)
			{
				const int centerX = static_cast<int>(meteor.x + meteor.size / 2);
				render.DrawMeteorWarning(mdc, centerX, meteor.lineY);

				if (meteor.meteorActive)
				{
					render.DrawSprite(mdc, meteorImage[meteor.animationCnt],
						meteor.x, meteor.y, meteor.size, meteor.size,
						meteorImageW, meteorImageH);
				}
			}

			// 운석에 맞았을 때 플레이어 위치에 폭발을 표시합니다.
			if (meteor.explosion)
			{
				render.DrawSprite(mdc, playerExplo[meteor.explosionCnt],
					player.x, player.y, 100, 100, playerExW, playerExH);
			}

			// 부활 효과
			if (player.revive)
			{
				render.DrawSprite(mdc, reviveImage[player.reviveAnimation],
					player.x, player.y, 100, 100, reviveImageW, reviveImageH);
			}

			// 아이템이 나타나기 전 효과
			for (int i = 0; i < MAX_ITEM; ++i)
			{
				if (!itemEffects[i].active)
					continue;

				const int frame = itemEffects[i].animation_count % 4;
				CImage& image = item_effectImage[frame];

				if (!image.IsNull())
				{
					render.DrawSprite(mdc, image,
						itemEffects[i].x, itemEffects[i].y, 100, 100,
						image.GetWidth(), image.GetHeight());
				}
			}

			// 바닥에 있는 아이템
			for (int i = 0; i < MAX_ITEM; ++i)
			{
				if (!items[i].active)
					continue;

				const auto& item = items[i];
				const float size = item.size - 10;

				render.DrawSprite(mdc, itemImages[item.type],
					item.x, item.y, size, size,
					item_image_W[item.type], item_image_H[item.type]);
			}


			if (gameState.fogAlpha > 0)
			{
				render.DrawFog(mdc, fogImage, client.right, client.bottom,
					static_cast<BYTE>(gameState.fogAlpha));
			}

			if (fade_alpha > 0 && !background_changed)
			{
				render.DrawFade(mdc, client.right, client.bottom,
					static_cast<BYTE>(fade_alpha));
			}

			const int iconSize = 55;
			const int spacing = 6;
			int iconX = client.right - iconSize - spacing;
			const int iconY = 10;

			if (gameState.fogActive)
			{
				// 보스 영역을 가리는 구름을 먼저 그립니다.
				render.DrawSprite(mdc, itemImages[ITEM_FOG],
					0, -70, client.right + 100, 400,
					item_image_W[ITEM_FOG], item_image_H[ITEM_FOG]);

				render.DrawStatusIcon(mdc, itemImages[ITEM_FOG],
					iconX, iconY, iconSize,
					item_image_W[ITEM_FOG], item_image_H[ITEM_FOG]);

				iconX -= iconSize + spacing;
			}

			if (player.speedTicks > 0)
			{
				render.DrawStatusIcon(mdc, itemImages[ITEM_SPEED],
					iconX, iconY, iconSize,
					item_image_W[ITEM_SPEED], item_image_H[ITEM_SPEED]);
			}

			if (bGamePause)
			{
				render.DrawPause(mdc, client.right, client.bottom);
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
    case ClientNetwork::LobbyChangedMessage:
        myPlayerNumber = static_cast<int>(wParam);
        hostPlayerNumber = static_cast<int>(lParam & 0xff);
        occupied[0] = ((lParam >> 8) & 0xff) != 0;
        occupied[1] = ((lParam >> 16) & 0xff) != 0;
        if (!occupied[0] || !occupied[1])
            remoteVisible = false;
        InvalidateRect(hWnd, nullptr, FALSE);
        break;

    case ClientNetwork::ConnectionLostMessage:
        remoteVisible = false;
        myPlayerNumber = 0;
        hostPlayerNumber = 0;
        occupied[0] = occupied[1] = false;
        InvalidateRect(hWnd, nullptr, FALSE);
        break;

    case ClientNetwork::PositionChangedMessage:
    {
        auto* network = reinterpret_cast<ClientNetwork*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        Protocol::PlayerPosition position;
        if (bGameStart && !bGameOver && occupied[0] && occupied[1] && network &&
            network->GetRemotePosition(position) && position.playerNumber != myPlayerNumber)
        {
            gameState.players[1].x = position.x;
            gameState.players[1].y = position.y;
            remoteVisible = true;
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        POINT mouse{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (!bGameStart && !bGameOver && myPlayerNumber != 0 &&
            myPlayerNumber == hostPlayerNumber && occupied[0] && occupied[1] &&
            PtInRect(&startBox, mouse))
        {
            auto* network = reinterpret_cast<ClientNetwork*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            if (network && !network->RequestStart())
                OutputDebugStringW(L"[Network] Start request failed\n");
        }
        break;
    }

    case ClientNetwork::GameStartedMessage:
        if (bGameStart || bGameOver)
            break;
        input.Reset();
        ResetGame(gameState, {client.left, client.top, client.right, client.bottom});
        bGameStart = true;
        bGamePause = false;
        remoteVisible = false;
        gameState.players[0].x += myPlayerNumber == 1 ? -70.0f : 70.0f;
        if (auto* network = reinterpret_cast<ClientNetwork*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA)))
            network->SetLocalPosition(gameState.players[0].x, gameState.players[0].y);
        PlaySound(Assets::background_bgm().c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_LOOP);
        SetTimer(hWnd, 1, 17, nullptr);
        SetTimer(hWnd, 2, 17, nullptr);
        SetTimer(hWnd, 3, 100, nullptr);
        InvalidateRect(hWnd, nullptr, FALSE);
        break;

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

				ResetGame(gameState, {client.left, client.top, client.right, client.bottom});
				back_scrollY = 0.0f;

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
		const GameBounds bounds{client.left, client.top, client.right, client.bottom};
		switch (wParam)
		{
		case 1:
			if (UpdatePlayer(gameState, bounds, input.ConsumeCommand()))
				mciSendString(L"play shotSound from 0", NULL, 0, NULL);
            if (auto* network = reinterpret_cast<ClientNetwork*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA)))
                network->SetLocalPosition(gameState.players[0].x, gameState.players[0].y);
			break;
		case 2:
		{
			const WorldStep step = UpdateWorld(gameState, bounds);
			for (GameEvent event : step.events)
				PlayGameEvent(event);

			if (step.outcome != RoundOutcome::Running)
			{
				bGameClear = step.outcome == RoundOutcome::Won;
				input.Reset();
				bGameOver = true;
                if (auto* network = reinterpret_cast<ClientNetwork*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA)))
                    network->StopPositionUpdates();
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

				if (bGameClear)
				{
					mciSendString(L"play winSound from 0", NULL, 0, NULL);
				}
				else
				{
					mciSendString(L"play loseSound from 0", NULL, 0, NULL);
				}
				break;
			}

			Change_Background_Fade(gameState.enemy.hp);
			// 위에서 아래로
			back_scrollY += back_scroll_speed * OBJECT_MOVE_RATIO;
			if (back_scrollY >= back[current_back].GetHeight())
			{
				back_scrollY -= back[current_back].GetHeight();
			}
			break;
		}
		case 3:
			for (GameEvent event : UpdateGameClock(gameState, bounds))
				PlayGameEvent(event);
			break;
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

		back[0].Destroy();
		back[1].Destroy();
		playerImage.Destroy();
		enemyImage.Destroy();
		enemyImage2.Destroy();
		enemyImage3.Destroy();
		bulletImage.Destroy();
		GameStartBGImage.Destroy();
		GameStartImage.Destroy();
		EmptyPlayerSlotImage.Destroy();
		ReadyPlayerSlotImage.Destroy();
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