// 11.11 생성
#include "ClientNetwork.h"
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")
char* SERVERIP = (char*)"127.0.0.1";

ClientNetwork::~ClientNetwork()
{
    Disconnect();
}

bool ClientNetwork::Connect(const char* ipv4, unsigned short port)
{
    Disconnect();

    int retval;

    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;
    winsockStarted_ = true;

    // 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) exit(1);
    
    // connect()
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ipv4, &serveraddr.sin_addr);
    serveraddr.sin_port = htons(port);
    retval = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) exit(1);

    return true;
}

void ClientNetwork::Disconnect()
{
    if (sock != INVALID_SOCKET)
    {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    if (winsockStarted_)
    {
        WSACleanup();
        winsockStarted_ = false;
    }
}