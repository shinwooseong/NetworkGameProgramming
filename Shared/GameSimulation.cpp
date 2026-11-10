#include "GameSimulation.h"


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