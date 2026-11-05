#pragma once
#include <array>

constexpr int MAX_PLAYERS = 2;

struct PlayerState
{
	float x = 0.0f;
	float y = 0.0f;
	float size = 0.0f;	// 충돌 판정 사용 크기

	int hp = 10;	// 캐릭터 hp
	int score = 0;	// 개인별 점수

	bool alive = true;  // 살아있나 체크


};

struct GameState
{
	PlayerState players[MAX_PLAYERS];
};