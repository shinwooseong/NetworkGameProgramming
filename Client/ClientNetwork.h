// 11.11 생성
#pragma once
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
    //소켓 번호
    SOCKET sock = INVALID_SOCKET;
    //WSAStartup 성공 여부
    bool winsockStarted_ = false;
};