// 11.11 생성
#pragma once
#include <stop_token>
#include <thread>
#include <winsock2.h>
#pragma comment(lib, "ws2_32")

class ClientNetwork
{
public:
    ClientNetwork() = default;
    ~ClientNetwork();

    ClientNetwork(const ClientNetwork&) = delete;
    ClientNetwork& operator=(const ClientNetwork&) = delete;

    bool Connect(const char* ipv4, unsigned short port);
    void Disconnect();

private:
    SOCKET sock = INVALID_SOCKET;    //소켓 번호
    bool winsockStarted = false;    //WSAStartup 성공 여부

    void ReceiveLoop(std::stop_token stop, SOCKET connectedSocket); // 대기방 패킷 수신 함수
    std::jthread receiveThread;                                    // 클라이언트 수신 스레드
};