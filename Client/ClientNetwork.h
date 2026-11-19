// 11.11 생성
#pragma once
#include <stop_token>
#include <thread>
#include <winsock2.h>
#include <atomic>
#pragma comment(lib, "ws2_32")

class ClientNetwork
{
public:
    ClientNetwork() = default;
    ~ClientNetwork();

    ClientNetwork(const ClientNetwork&) = delete;
    ClientNetwork& operator=(const ClientNetwork&) = delete;

    unsigned int GetPlayerSlots() const
    {
        return playerSlots.load();
    }



    bool Connect(const char* ipv4, unsigned short port);
    void Disconnect();

private:
    SOCKET sock = INVALID_SOCKET;    //소켓 번호
    bool winsockStarted = false;    //WSAStartup 성공 여부

    void ReceiveLoop(std::stop_token stop, SOCKET connectedSocket); // 대기방 패킷 수신 함수

    // 0 = 둘 다 비어 있음
    // 1 = 1번 접속, 2 = 2번 접속, 3 = 둘 다 접속
    std::atomic<unsigned int> playerSlots{ 0 };


    std::jthread receiveThread;                                    // 클라이언트 수신 스레드
};