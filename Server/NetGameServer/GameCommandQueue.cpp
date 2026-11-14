#include "GameCommandQueue.h"

void GameCommandQueue::Push(GameCommand command) 
{
    {
        std::lock_guard<std::mutex> lock(mutex); // 큐를 쓰기 전에 잠금
        commands.push(command);                 // 새 명령을 큐에 push
    } 

    ready.notify_one();                         // 게임 연산 스레드 깨우기
}

bool GameCommandQueue::WaitPop(std::stop_token stop, GameCommand& command)
{
    std::unique_lock<std::mutex> lock(mutex);    // 큐 잠금

    if (!ready.wait(lock, stop, [this] {
        return !commands.empty();           // 꺼낼 명령이 생기면 대기 종료
        }))
    {
        return false;                            // 종료 요청을 받았고 큐가 빔
    }

    command = commands.front();                 // 가장 먼저 들어온 명령 복사
    commands.pop();                             // 복사한 명령을 큐에서 제거
    return true;                                 // 명령 하나를 꺼냄
}  