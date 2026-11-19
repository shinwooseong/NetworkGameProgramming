#include "SendCommandQueue.h"
#include <utility>

void SendCommandQueue::Push(SendCommand command)
{
    {
        std::lock_guard<std::mutex> lock(mutex); // 큐를 쓰기 전에 잠금
        commands.push(std::move(command));      // 대상 세션과 패킷 소유권의 큐 전달
    }

    ready.notify_one();                         // 송신 스레드 깨우기
}

bool SendCommandQueue::WaitPop(std::stop_token stop, SendCommand& command)
{
    std::unique_lock<std::mutex> lock(mutex);    // 큐 잠금

    if (!ready.wait(lock, stop, [this] {
        return !commands.empty();           // 꺼낼 명령이 생기면 대기 종료
        }))
    {
        return false;                            // 종료 요청을 받았고 큐가 빔
    }

    command = std::move(commands.front());      // 가장 먼저 들어온 명령의 소유권 이동
    commands.pop();                             // 복사한 명령을 큐에서 제거
    return true;                                 // 명령 하나를 꺼냄
}
