#include "ClientNetwork.h"
#include "../Shared/Protocol.h"
#include <ws2tcpip.h>
#include <cstdio>

#pragma comment(lib, "Ws2_32.lib")

ClientNetwork::~ClientNetwork()
{
    Disconnect();
}

bool ClientNetwork::Connect(const char* ipv4, unsigned short port, HWND window)
{
    Disconnect();
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;
    winsockStarted = true;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (!ipv4 || InetPtonA(AF_INET, ipv4, &address.sin_addr) != 1)
    {
        Disconnect();
        return false;
    }
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET ||
        connect(sock, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        Disconnect();
        return false;
    }
    const DWORD timeout = 1000; // 시작 요청 송신 제한시간
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    window_ = window;
    receiveThread = std::jthread([this, connectedSocket = sock](std::stop_token stop)
    {
        ReceiveLoop(stop, connectedSocket);
    });
    return true;
}

bool ClientNetwork::RequestStart()
{
    if (sock == INVALID_SOCKET)
        return false;
    const Protocol::StartRequestPacket packet{Protocol::StartRequestType};
    return send(sock, reinterpret_cast<const char*>(packet.data()),
        static_cast<int>(packet.size()), 0) == static_cast<int>(packet.size());
}

void ClientNetwork::Disconnect()
{
    if (receiveThread.joinable())
    {
        receiveThread.request_stop();
        shutdown(sock, SD_BOTH); // 대기 중인 수신의 반환 유도
        receiveThread.join();
    }
    if (sock != INVALID_SOCKET)
    {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    if (winsockStarted)
    {
        WSACleanup();
        winsockStarted = false;
    }
    window_ = nullptr;
}

void ClientNetwork::ReceiveLoop(std::stop_token stop, SOCKET connectedSocket)
{
    Protocol::LobbyPacket packet{}; // 현재 서버 메시지의 최대 길이
    int filled = 0;
    int expected = 1; // 첫 바이트의 타입 선행 수신
    while (!stop.stop_requested())
    {
        const int received = recv(connectedSocket,
            reinterpret_cast<char*>(packet.data() + filled), expected - filled, 0);
        if (stop.stop_requested())
            return;
        if (received <= 0)
            break;
        filled += received;
        if (filled < expected)
            continue;

        if (packet[Protocol::Type] == Protocol::LobbyStateType)
        {
            expected = Protocol::LobbyPacketSize;
            if (filled < expected)
                continue;
            char message[128]{};
            std::snprintf(message, sizeof(message), "[Lobby] My=%d Host=%d P1=%d P2=%d\n",
                packet[Protocol::MyPlayerNumber], packet[Protocol::HostNumber],
                packet[Protocol::Player1Occupied], packet[Protocol::Player2Occupied]);
            OutputDebugStringA(message);
            const LPARAM state = packet[Protocol::HostNumber] |
                (packet[Protocol::Player1Occupied] << 8) | (packet[Protocol::Player2Occupied] << 16);
            PostMessageW(window_, LobbyChangedMessage, packet[Protocol::MyPlayerNumber], state);
        }
        else if (packet[Protocol::Type] == Protocol::GameStartedType)
        {
            OutputDebugStringA("[Game] Server approved start\n");
            PostMessageW(window_, GameStartedMessage, 0, 0);
        }
        else
        {
            OutputDebugStringA("[Network] Unknown server message\n");
            break;
        }
        filled = 0;
        expected = 1;
    }
    shutdown(connectedSocket, SD_BOTH);
    PostMessageW(window_, ConnectionLostMessage, 0, 0);
}
