// 11.11 생성
#include "ClientNetwork.h"
#include <ws2tcpip.h>

#include <array>
#include <cstdio>

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
    winsockStarted = true;

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

    // 연결 소켓의 수신 스레드 시작
    receiveThread = std::jthread(
        [this, connectedSocket = sock](std::stop_token stop)
        {
            ReceiveLoop(stop, connectedSocket); // 대기방 패킷 수신
            

        });

    return true;
}

void ClientNetwork::Disconnect()
{
    receiveThread.request_stop();

    if (sock != INVALID_SOCKET)
        shutdown(sock, SD_BOTH);

    if (receiveThread.joinable())
        receiveThread.join();

    if (sock != INVALID_SOCKET)
    {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    playerSlots.store(0);
    hostNumber.store(0);

    if (winsockStarted)
    {
        WSACleanup();
        winsockStarted = false;
    }

}

void ClientNetwork::ReceiveLoop(std::stop_token stop, SOCKET connectedSocket)
{
    constexpr unsigned char LobbyStateType = 1; // 대기방 메시지 타입
    constexpr int PacketSize = 5;               // 대기방 패킷 길이
    std::array<unsigned char, PacketSize> packet{}; // 패킷 수신 버퍼
    int filled = 0;                                 // 현재까지 수신한 바이트 수




    while (!stop.stop_requested())
    {
        const int received = recv(
            connectedSocket,
            reinterpret_cast<char*>(packet.data() + filled),
            PacketSize - filled,
            0); // 패킷의 남은 바이트 수신

        if (stop.stop_requested())
            break; // 클라이언트 종료 요청 확인

        if (received == 0)
        {
            OutputDebugStringA(filled == 0
                ? "[Lobby] Server closed the connection\n"
                : "[Lobby] Connection closed mid-packet\n"); // 서버 연결 종료 기록
            break;
        }

        if (received == SOCKET_ERROR)
        {
            const int error = WSAGetLastError(); // 수신 오류 번호 조회
            char message[80]{};
            std::snprintf(message, sizeof(message),
                "[Lobby] recv failed: %d\n", error);
            OutputDebugStringA(message); // 수신 오류 기록
            break;
        }

        filled += received; // 수신 위치 갱신
        if (filled < PacketSize)
            continue; // 남은 바이트의 추가 수신

        if (packet[0] != LobbyStateType)
        {
            OutputDebugStringA("[Lobby] Unexpected packet type\n"); // 패킷 타입 불일치 기록
            break;
        }

        char message[128]{};
        std::snprintf(message, sizeof(message),
            "[Lobby] My=%d Host=%d P1=%d P2=%d\n",
            static_cast<int>(packet[1]),
            static_cast<int>(packet[2]),
            static_cast<int>(packet[3]),
            static_cast<int>(packet[4]));
        OutputDebugStringA(message); // 완성된 대기방 상태 출력

        const unsigned int slots =
            (packet[3] != 0 ? 1u : 0u) |
            (packet[4] != 0 ? 2u : 0u);

        playerSlots.store(slots);
        hostNumber.store(packet[2]);

        filled = 0; // 다음 패킷의 수신 위치 초기화
    }


    // 서버 연결이 끊기면 두 자리 모두 비운다.
    playerSlots.store(0);
    hostNumber.store(0);
}