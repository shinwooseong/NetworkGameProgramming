#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>

#include <cstdint>
#include <memory>
#include <array>
#include <WinSock2.h>

constexpr std::uint8_t LobbyStateType = 1; // 대기방 상태 메시지 번호

using LobbyPacket = std::array<std::uint8_t, 5>; // 고정 5바이트 패킷

struct ClientSession
{
    explicit ClientSession(SOCKET socket) : socket(socket) {} // 연결 소켓 인수
    ~ClientSession() { closesocket(socket); }                  // 마지막 참조 해제 시 소켓 정리

    ClientSession(const ClientSession&) = delete;            // 소켓 소유권 복사 금지
    ClientSession& operator=(const ClientSession&) = delete; // 소켓 소유권 대입 금지

    const SOCKET socket; // 접속별 통신 소켓
};

struct SendCommand
{
    std::shared_ptr<ClientSession> target; // 송신 대상 연결
    LobbyPacket packet;                   // 송신할 패킷 사본
};
class SendCommandQueue
{
public:
    void Push(SendCommand command);
    bool WaitPop(std::stop_token stop, SendCommand& command);

private:
    std::queue<SendCommand> commands;
    std::mutex mutex;
    std::condition_variable_any ready;
};

