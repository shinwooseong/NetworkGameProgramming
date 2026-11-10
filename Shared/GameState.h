#pragma once

constexpr int MAX_PLAYERS = 2;
constexpr int MAX_BULLET = 10;
constexpr int MAX_REDCIRCLE = 10;
constexpr int MAX_ITEM = 15;

struct PlayerState
{
    float x = 0.0f;
    float y = 0.0f;
    int size = 100;
    int hp = 10;
    int score = 0;
    bool alive = true;
    bool hasRebirth = false;
    bool revive = false;
    int reviveAnimation = 0;
    int reviveTicks = 0;
    bool blinking = false;          // 아이템 먹고 깜빡임
    int blinkTicks = 0;
    float speed = 5.0f;
    int speedTicks = 0;
};

struct Bullet
{
    float x, y;
    float size;
    float speed;
    bool active; // 총알 활성화 ( active시 출력 )

    bool explosion; // true시 폭발 이미지 출력
    float exploX, exploY; // 폭발 이미지 크기
    int explosionCnt = 0; // 애니메이션
};

struct Enemy
{
    float x, y; // 위치
    float size; // 크기
    float dx, dy; // 방향 변수
    float speed; // 속도
    int directionChangeCounter; // 랜덤 방향 이동을 위한 변수

    int hp; //
};

struct Obstacle
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
};

struct Meteor
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
};

enum ItemType
{
    ITEM_HEAL,        // 힐팩
    ITEM_SPEED,        // 속도 증가
    ITEM_POSION,    // 독
    ITEM_FOG,        // 안개
    ITEM_REBIRTH    // 부활
};

struct Item
{
    float x, y;        // 생성 위치
    float size;        // 아이템 크기
    bool active;    // 아이템 활성화
    ItemType type;    // 아이템 종류
};

struct ItemEffect
{
    float x, y;
    bool active;
    int animation_count;    // 애니메이션 프레임
    int duration;    // 이펙트 지속시간
};

struct GameState
{
    PlayerState players[MAX_PLAYERS]{};
    Enemy enemy{};
    Bullet bullets[MAX_BULLET]{};
    int nextBullet = 0;
    Obstacle redCircles[MAX_REDCIRCLE]{};
    Meteor meteor{};
    Item items[MAX_ITEM]{};
    ItemEffect itemEffects[MAX_ITEM]{};

    float playTime = 0.0f;
    int redCircleTicks = 0;
    int meteorTicks = 0;
    int itemTicks = 0;
    bool fogActive = false;
    int fogTicks = 0;
    int fogAlpha = 0;
    bool fogFadeIn = false;
    bool fogFadeOut = false;
};
