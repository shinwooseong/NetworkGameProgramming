#include "ClientNetwork.h"
#include "../Shared/Protocol.h"
#include <ws2tcpip.h>
#include <cstdio>
#include <chrono>

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
    const BOOL noDelay = TRUE; // 짧은 좌표 패킷의 지연 없는 송신
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
    window_ = window;
    receiveThread = std::jthread([this, connectedSocket = sock](std::stop_token stop)
    {
        ReceiveLoop(stop, connectedSocket);
    });
    sendThread = std::jthread([this, connectedSocket = sock](std::stop_token stop)
    {
        SendLoop(stop, connectedSocket);
    });
    return true;
}

bool ClientNetwork::RequestStart()
{
    if (sock == INVALID_SOCKET)
        return false;
    {
        std::lock_guard lock(stateMutex);
        startRequested = true;
    }
    sendReady.notify_one();
    return true;
}

void ClientNetwork::SetLocalPosition(float x, float y)
{
    std::lock_guard lock(stateMutex);
    localX = x;
    localY = y;
    positionEnabled = true;
}

void ClientNetwork::StopPositionUpdates()
{
    std::lock_guard lock(stateMutex);
    positionEnabled = false;
}

void ClientNetwork::SetLocalScore(int score)
{
    if (score < 0) {
        return;
    }

    {
        std::lock_guard lock(stateMutex);

        if (localScore == score) {
            return;
        }

        localScore = score;
        scorePending = true;
    }

    sendReady.notify_one();

}

bool ClientNetwork::GetRemotePosition(Protocol::PlayerPosition& position)
{
    std::lock_guard lock(stateMutex);
    position = remotePosition;
    return hasRemotePosition;
}

void ClientNetwork::SendLoop(std::stop_token stop, SOCKET connectedSocket)
{
    using Clock = std::chrono::steady_clock;
    const auto interval = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / 60.0)); // 초당 60회 송신 간격
    auto nextSend = Clock::now();
    while (!stop.stop_requested())
    {
        Protocol::Packet packet;
        {
            std::unique_lock lock(stateMutex);
            sendReady.wait_until(lock, stop, nextSend, [this] { return startRequested || scorePending; });
            if (stop.stop_requested())
                break;
            if (startRequested)
            {
                packet = Protocol::SerializePacket(Protocol::StartRequestPacket{Protocol::StartRequestType});
                startRequested = false;
            }
            else if (scorePending)
            {
                packet = Protocol::SerializePacket(
                    Protocol::MakeScorePacket(0, localScore));

                scorePending = false;
            }
            else
            {
                if (positionEnabled)
                    packet = Protocol::SerializePacket(Protocol::MakePositionPacket(0, localX, localY));
                nextSend += interval;
                if (nextSend <= Clock::now())
                    nextSend = Clock::now() + interval; // 지연 후 몰아서 전송하는 동작 방지
            }
        }
        std::size_t sent = 0;
        while (sent < packet.size() && !stop.stop_requested())
        {
            const int count = send(connectedSocket,
                reinterpret_cast<const char*>(packet.data() + sent),
                static_cast<int>(packet.size() - sent), 0);
            if (count <= 0)
            {
                shutdown(connectedSocket, SD_BOTH);
                return;
            }
            sent += static_cast<std::size_t>(count);
        }
    }
}

void ClientNetwork::Disconnect()
{
    receiveThread.request_stop();
    sendThread.request_stop();
    if (sock != INVALID_SOCKET)
        shutdown(sock, SD_BOTH); // 대기 중인 송수신의 반환 유도
    if (sendThread.joinable())
        sendThread.join();
    if (receiveThread.joinable())
        receiveThread.join();
    {
        std::lock_guard lock(stateMutex);
        startRequested = false;
        positionEnabled = false;
        hasRemotePosition = false;
        localScore = -1;
        scorePending = false;
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
    Protocol::PositionPacket packet{}; // 현재 서버 메시지의 최대 길이
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

        if (filled == 1)
        {
            switch (packet[0])
            {
            case Protocol::LobbyStateType: expected = Protocol::LobbyPacketSize; break;
            case Protocol::GameStartedType: expected = 1; break;
            case Protocol::PlayerPositionType: expected = Protocol::PositionPacketSize; break;
            case Protocol::PlayerScoreType: expected = Protocol::ScorePacketSize; break;
            default:
                shutdown(connectedSocket, SD_BOTH);
                PostMessageW(window_, ConnectionLostMessage, 0, 0);
                return;
            }
            if (filled < expected)
                continue;
        }
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
        else if (packet[0] == Protocol::PlayerPositionType)
        {
            Protocol::PlayerPosition position;
            if (!Protocol::ReadPosition(packet.data(), position) ||
                position.playerNumber < 1 || position.playerNumber > 2)
                break;
            {
                std::lock_guard lock(stateMutex);
                remotePosition = position;
                hasRemotePosition = true;
            }
            PostMessageW(window_, PositionChangedMessage, 0, 0);
        }
        else if (packet[0] == Protocol::PlayerScoreType)
        {
            Protocol::PlayerScore score;
            if (!Protocol::ReadScore(packet.data(), score) ||
                score.playerNumber < 1 || score.playerNumber > 2)
                break;

            PostMessageW(window_, ScoreChangedMessage,
                static_cast<WPARAM>(score.playerNumber),
                static_cast<LPARAM>(score.score));
        }
        filled = 0;
        expected = 1;
    }
    shutdown(connectedSocket, SD_BOTH);
    PostMessageW(window_, ConnectionLostMessage, 0, 0);
}
