#pragma once
#include "InputCommand.h"
#include "GameState.h"



// 플레이어 이동계산
void MovePlayer(float& x, float& y, float size, float speed,
    const InputCommand& command,
    int left, int right, int top, int bottom);


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



struct BulletHitResult
{
    bool hit = false;
    bool defeated = false;
};

// 총알과 보스 충돌 처리
BulletHitResult CheckEnemyBulletCollision(
    Bullet(&bullets)[MAX_BULLET],
    float enemyX, float enemyY, float enemySize,
    int& enemyHp, int& score);



// 출발 위치와 발사 상태 계산
bool UpdatePlayerFire(Bullet(&bullets)[MAX_BULLET], int& bulletCnt,
    float playerX, float playerY, int playerSize, bool firePressed);

// 발사된 뒤의 위치 계산
void MoveBullets(Bullet(&bullets)[MAX_BULLET], float moveRatio);