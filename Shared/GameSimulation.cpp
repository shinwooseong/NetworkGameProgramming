#include "GameSimulation.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>


namespace
{
constexpr float ObjectMoveRatio = 17.0f / 31.0f;

struct BulletHitResult
{
    bool hit = false;
    bool defeated = false;
};

// 플레이어 이동계산
void MovePlayer(float& x, float& y, float size, float speed,
    const InputCommand& command,
    int left, int right, int top, int bottom)
{
    if (command.up && y > top)
        y -= speed;

    if (command.down && y + size < bottom)
        y += speed;

    if (command.right && x + size < right)
        x += speed;

    if (command.left && x > left)
        x -= speed;
}


// 출발 위치와 발사 상태 정함
bool UpdatePlayerFire(Bullet(&bullets)[MAX_BULLET], int& bulletCnt,
    float playerX, float playerY, int playerSize, bool firePressed)
{
    for (Bullet& bullet : bullets)
    {
        if (!bullet.active)
        {
            bullet.x = playerX + playerSize / 2 - 15;
            bullet.y = playerY;
        }
    }

    if (!firePressed)
        return false;

    bullets[bulletCnt].active = true;
    bulletCnt = (bulletCnt + 1) % MAX_BULLET;
    return true;
}

// 발사된 뒤의 위치 계산
void MoveBullets(Bullet(&bullets)[MAX_BULLET], float moveRatio)
{
    const float move = 15.0f;

    for (Bullet& bullet : bullets)
    {
        if (bullet.active)
            bullet.y -= bullet.speed * move * moveRatio;

        if (bullet.y < 0.0f)
            bullet.active = false;
    }
}


// 총알과 보스 충돌 처리
BulletHitResult CheckEnemyBulletCollision(
    Bullet(&bullets)[MAX_BULLET],
    float enemyX, float enemyY, float enemySize,
    int& enemyHp, int& score)
{
    BulletHitResult result;

    const int enemyLeft = static_cast<int>(enemyX);
    const int enemyTop = static_cast<int>(enemyY);
    const int enemyRight = static_cast<int>(enemyX + enemySize);
    const int enemyBottom = static_cast<int>(enemyY + enemySize - 40);

    for (Bullet& bullet : bullets)
    {
        if (!bullet.active)
            continue;

        const int left = static_cast<int>(bullet.x);
        const int top = static_cast<int>(bullet.y);
        const int right = static_cast<int>(bullet.x + bullet.size);
        const int bottom = static_cast<int>(bullet.y + bullet.size);

        const bool overlaps =
            std::max(left, enemyLeft) < std::min(right, enemyRight) &&
            std::max(top, enemyTop) < std::min(bottom, enemyBottom);

        if (!overlaps)
            continue;

        --enemyHp;
        score += 10;
        result.hit = true;

        bullet.active = false;
        bullet.explosion = true;
        bullet.exploX = bullet.x;
        bullet.exploY = bullet.y;

        if (enemyHp == 0)
        {
            score += 100;
            result.defeated = true;
            return result;
        }
    }

    for (Bullet& bullet : bullets)
    {
        if (!bullet.explosion)
            continue;

        if (++bullet.explosionCnt == 3)
        {
            bullet.explosionCnt = 0;
            bullet.explosion = false;
            bullet.exploX = 0;
            bullet.exploY = 0;
        }
    }

    return result;
}

// 보스가 다음에 움직일 방향 무작위 지정
void ChooseEnemyDirection(Enemy& enemy)
{
    constexpr double pi = 3.14159265;
    const float angle = static_cast<float>((std::rand() % 360) * pi / 180.0);
    enemy.dx = std::cos(angle);
    enemy.dy = std::sin(angle);
    enemy.directionChangeCounter = 0;
}

// 실제 보스 움직임
void MoveEnemy(Enemy& enemy, int right, int bottom, float moveRatio)
{
    enemy.x += enemy.speed * enemy.dx * moveRatio;
    enemy.y += enemy.speed * enemy.dy * moveRatio;

    ++enemy.directionChangeCounter;
    if (enemy.directionChangeCounter > 50)
        ChooseEnemyDirection(enemy);

    if (enemy.x < 0 || enemy.x + enemy.size > right)
        enemy.dx *= -1;

    if (enemy.y < 0 || enemy.y + enemy.size > bottom)
        enemy.dy *= -1;
}

struct HitBox
{
    int left, top, right, bottom;
};

HitBox MakeHitBox(float x, float y, float size, int margin = 0)
{
    return {static_cast<int>(x + margin), static_cast<int>(y + margin),
        static_cast<int>(x + size - margin), static_cast<int>(y + size - margin)};
}

// 히트박스 겹치는지 확인하는 함수
bool Overlaps(const HitBox& a, const HitBox& b)
{
    return std::max(a.left, b.left) < std::min(a.right, b.right) &&
        std::max(a.top, b.top) < std::min(a.bottom, b.bottom);
}

// 빨간공 움직임
void MoveRedCircles(GameState& state, const GameBounds& bounds)
{
    for (Obstacle& circle : state.redCircles)
    {
        if (circle.active)
        {
            circle.x += circle.speed * circle.dx * ObjectMoveRatio;
            circle.y += circle.speed * circle.dy * ObjectMoveRatio;
        }
        if (circle.x > bounds.right || circle.x < bounds.left || circle.y > bounds.bottom)
            circle.active = false;
    }
}

// 메테오 움직임
void MoveMeteor(Meteor& meteor, int bottom)
{
    if (meteor.meteorActive)
    {
        meteor.y += 10.0f * ObjectMoveRatio;
        meteor.animationCnt = (meteor.animationCnt + 1) % 3;
    }
    if (meteor.y >= bottom)
    {
        meteor.meteorActive = false;
        meteor.lineActive = false;
        meteor.y = 0;
        meteor.lineY = 0;
    }
}

// 빨간 원이나 운석에 맞았다고 판정된 뒤, 플레이어가 받은 피해를 처리
bool ApplyObstacleHit(PlayerState& player, std::vector<GameEvent>& events)
{
    --player.hp;
    events.push_back(GameEvent::PlayerHit);
    if (player.hasRebirth && player.hp <= 0)
    {
        player.hp = 5;
        player.hasRebirth = false;
        player.revive = true;
        player.reviveAnimation = 0;
        events.push_back(GameEvent::Revived);
    }
    return player.hp <= 0;
}

bool CheckRedCircleHits(GameState& state, std::vector<GameEvent>& events)
{
    PlayerState& player = state.players[0];
    const Enemy& enemy = state.enemy;
    const HitBox playerBox = MakeHitBox(player.x, player.y, player.size, 20);
    for (Obstacle& circle : state.redCircles)
    {
        if (!circle.active || !Overlaps(MakeHitBox(circle.x, circle.y, circle.size), playerBox))
            continue;

        if (ApplyObstacleHit(player, events))
            return true;

        circle.active = false;
        circle.explosion = true;
        circle.exploX = circle.x;
        circle.exploY = circle.y;
        circle.explosionCnt = 0;
        circle.x = enemy.x + enemy.size / 2;
        circle.y = enemy.y + enemy.size;
    }
    return false;
}

bool CheckMeteorHit(GameState& state, std::vector<GameEvent>& events)
{
    PlayerState& player = state.players[0];
    Meteor& meteor = state.meteor;
    if (!meteor.meteorActive || !Overlaps(MakeHitBox(meteor.x, meteor.y, meteor.size, 20),
        MakeHitBox(player.x, player.y, player.size)))
        return false;

    if (ApplyObstacleHit(player, events))
        return true;

    meteor.meteorActive = false;
    meteor.lineActive = false;
    meteor.lineY = 0;
    meteor.explosion = true;
    meteor.exploX = meteor.x + meteor.size / 2;
    meteor.exploY = meteor.y;
    meteor.explosionCnt = 0;
    meteor.y = 0;
    return false;
}

bool ApplyItem(GameState& state, ItemType type, std::vector<GameEvent>& events)
{
    PlayerState& player = state.players[0];
    switch (type)
    {
    case ITEM_HEAL:
        if (player.hp < 10)
            player.hp += 2;
        events.push_back(GameEvent::Healed);
        break;
    case ITEM_SPEED:
        player.speed = 12.5f;
        player.speedTicks = 50;
        events.push_back(GameEvent::SpeedStarted);
        break;
    case ITEM_POSION:
        player.hp -= 2;
        if (player.hp <= 0 && !player.hasRebirth)
            return true;
        events.push_back(GameEvent::Poisoned);
        break;
    case ITEM_FOG:
        state.fogFadeIn = true;
        state.fogAlpha = 0;
        state.fogTicks = 50;
        state.fogActive = true;
        events.push_back(GameEvent::FogStarted);
        break;
    case ITEM_REBIRTH:
        player.hasRebirth = true;
        events.push_back(GameEvent::Healed);
        break;
    }
    return false;
}

bool CheckItemPickups(GameState& state, std::vector<GameEvent>& events)
{
    PlayerState& player = state.players[0];
    const HitBox playerBox = MakeHitBox(player.x, player.y, player.size, 10);
    for (Item& item : state.items)
    {
        if (!item.active || !Overlaps(MakeHitBox(item.x, item.y, item.size), playerBox))
            continue;

        const bool died = ApplyItem(state, item.type, events);
        item.active = false;
        player.blinking = true;
        player.score += 30;
        if (died)
            return true;
    }
    return false;
}

void SpawnRedCircle(GameState& state)
{
    const PlayerState& player = state.players[0];
    const Enemy& enemy = state.enemy;
    for (Obstacle& circle : state.redCircles)
    {
        if (circle.active)
            continue;

        circle.active = true;
        circle.x = enemy.x + enemy.size / 2 - 10;
        circle.y = enemy.y + enemy.size - 10;
        const float vx = player.x + player.size / 2 - circle.x;
        const float vy = player.y + player.size / 2 - circle.y;
        const float length = std::sqrt(vx * vx + vy * vy);
        circle.dx = length > 0 ? vx / length : 0.0f;
        circle.dy = length > 0 ? vy / length : 1.0f;
        break;
    }
}

void SpawnItem(GameState& state, const GameBounds& bounds)
{
    for (int i = 0; i < MAX_ITEM; ++i)
    {
        Item& item = state.items[i];
        ItemEffect& effect = state.itemEffects[i];
        if (item.active || effect.active)
            continue;

        item.x = static_cast<float>(std::rand() % std::max(1, bounds.right - 50));
        item.y = static_cast<float>(std::rand() % std::max(1, bounds.bottom - 100));
        item.size = 100.0f;
        const int probability = std::rand() % 100;
        if (probability < 20) item.type = ITEM_HEAL;
        else if (probability < 45) item.type = ITEM_SPEED;
        else if (probability < 50) item.type = ITEM_REBIRTH;
        else if (probability < 85) item.type = ITEM_POSION;
        else item.type = ITEM_FOG;
        effect = {item.x, item.y, true, 0, 30};
        break;
    }
}

template <typename Object>
void AdvanceExplosion(Object& object)
{
    if (object.explosion && ++object.explosionCnt == 3)
    {
        object.explosionCnt = 0;
        object.explosion = false;
        object.exploX = 0;
        object.exploY = 0;
    }
}
}

void ResetGame(GameState& state, const GameBounds& bounds)
{
    state = {};
    PlayerState& player = state.players[0];
    player.x = static_cast<float>(bounds.right / 2 - player.size / 2);
    player.y = static_cast<float>(bounds.bottom - 150);

    Enemy& enemy = state.enemy;
    enemy.size = 100;
    enemy.x = static_cast<float>(bounds.right / 2 - player.size);
    enemy.y = static_cast<float>(bounds.top + 30);
    enemy.speed = 3.0f;
    enemy.hp = 300;
    ChooseEnemyDirection(enemy);

    for (Bullet& bullet : state.bullets)
    {
        bullet.size = 30;
        bullet.speed = 1.0f;
        bullet.x = player.x + player.size / 2 - 10;
        bullet.y = player.y;
    }
    for (Obstacle& circle : state.redCircles)
    {
        circle.size = 30.0f;
        circle.speed = 3.0f;
        circle.x = enemy.x + enemy.size / 2;
        circle.y = enemy.y + enemy.size;
    }
    state.meteor.size = 150.0f;
}

bool UpdatePlayer(GameState& state, const GameBounds& bounds, const InputCommand& command)
{
    PlayerState& player = state.players[0];
    MovePlayer(player.x, player.y, player.size, player.speed,
        command, bounds.left, bounds.right, 150, bounds.bottom);
    return UpdatePlayerFire(state.bullets, state.nextBullet,
        player.x, player.y, player.size, command.firePressed);
}

WorldStep UpdateWorld(GameState& state, const GameBounds& bounds)
{
    WorldStep step;
    PlayerState& player = state.players[0];
    Enemy& enemy = state.enemy;
    MoveEnemy(enemy, bounds.right, 200, ObjectMoveRatio);
    MoveBullets(state.bullets, ObjectMoveRatio);
    MoveRedCircles(state, bounds);
    MoveMeteor(state.meteor, bounds.bottom);

    const BulletHitResult hit = CheckEnemyBulletCollision(
        state.bullets, enemy.x, enemy.y, enemy.size, enemy.hp, player.score);
    if (hit.hit)
        step.events.push_back(GameEvent::BossHit);

    const bool circleKilled = CheckRedCircleHits(state, step.events);
    const bool meteorKilled = CheckMeteorHit(state, step.events);
    const bool itemKilled = CheckItemPickups(state, step.events);
    player.alive = !(circleKilled || meteorKilled || itemKilled);
    if (hit.defeated || !player.alive)
    {
        step.outcome = hit.defeated ? RoundOutcome::Won : RoundOutcome::Lost;
        return step;
    }

    for (Item& item : state.items)
    {
        if (item.active)
        {
            item.y += 2.0f * ObjectMoveRatio;
            if (item.y > bounds.bottom)
                item.active = false;
        }
    }
    // Moving items can enter the hit box in the same tick.
    if (CheckItemPickups(state, step.events))
    {
        player.alive = false;
        step.outcome = RoundOutcome::Lost;
    }
    return step;
}

std::vector<GameEvent> UpdateGameClock(GameState& state, const GameBounds& bounds)
{
    std::vector<GameEvent> events;
    PlayerState& player = state.players[0];
    Meteor& meteor = state.meteor;
    state.playTime += 0.1;

    if (++state.redCircleTicks == 10)
    {
        SpawnRedCircle(state);
        state.redCircleTicks = 0;
    }
    if (state.enemy.hp <= 200 && !meteor.lineActive && ++state.meteorTicks >= 10)
    {
        meteor.lineActive = true;
        meteor.x = static_cast<float>(10 + std::rand() % std::max(1, bounds.right - 100));
        state.meteorTicks = 0;
    }
    if (meteor.lineActive)
    {
        meteor.lineY += 30;
        if (meteor.lineY >= bounds.bottom)
        {
            meteor.meteorActive = true;
            events.push_back(GameEvent::MeteorStarted);
        }
    }

    for (int i = 0; i < MAX_ITEM; ++i)
    {
        ItemEffect& effect = state.itemEffects[i];
        if (effect.active && ++effect.animation_count >= effect.duration)
        {
            effect.active = false;
            state.items[i].active = true;
        }
    }
    if (++state.itemTicks == 60)
    {
        SpawnItem(state, bounds);
        state.itemTicks = 0;
    }
    if (player.speedTicks > 0 && --player.speedTicks == 0)
    {
        player.speed = 5.0f;
        events.push_back(GameEvent::SpeedEnded);
    }

    if (state.fogFadeIn)
    {
        state.fogAlpha = std::min(255, state.fogAlpha + 10);
        state.fogFadeIn = state.fogAlpha < 255;
    }
    else if (state.fogTicks > 0)
    {
        if (--state.fogTicks == 0)
        {
            state.fogFadeOut = true;
            events.push_back(GameEvent::FogEnded);
        }
    }
    else if (state.fogFadeOut)
    {
        state.fogAlpha = std::max(0, state.fogAlpha - 10);
        if (state.fogAlpha == 0)
        {
            state.fogFadeOut = false;
            state.fogActive = false;
        }
    }

    for (Obstacle& circle : state.redCircles)
        AdvanceExplosion(circle);
    AdvanceExplosion(meteor);

    if (player.blinking && ++player.blinkTicks == 30)
    {
        player.blinking = false;
        player.blinkTicks = 0;
    }
    if (player.revive)
    {
        if (++player.reviveTicks == 2)
        {
            ++player.reviveAnimation;
            player.reviveTicks = 0;
        }
        if (player.reviveAnimation == 3)
        {
            player.revive = false;
            player.reviveAnimation = 0;
            player.reviveTicks = 0;
        }
    }
    return events;
}
