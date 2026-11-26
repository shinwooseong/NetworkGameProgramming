// 11.11 생성
#pragma once
#include <stop_token>
#include <thread>
#include <winsock2.h>
#include <condition_variable>
#include <mutex>
#include "../Shared/Protocol.h"
#pragma comment(lib, "ws2_32")

class ClientNetwork
{
public:
    static constexpr UINT LobbyChangedMessage = WM_APP + 1; // 로비 갱신 알림
    static constexpr UINT GameStartedMessage = WM_APP + 2; // 게임 시작 알림
    static constexpr UINT ConnectionLostMessage = WM_APP + 3; // 연결 종료 알림
    static constexpr UINT PositionChangedMessage = WM_APP + 4; // 상대 좌표 갱신 알림
    static constexpr UINT ScoreChangedMessage = WM_APP + 5; // 점수 변경


    ClientNetwork() = default;
    ~ClientNetwork();

    ClientNetwork(const ClientNetwork&) = delete;
    ClientNetwork& operator=(const ClientNetwork&) = delete;

    bool Connect(const char* ipv4, unsigned short port, HWND window = nullptr);
    bool RequestStart();
    void SetLocalPosition(float x, float y);
    void StopPositionUpdates();
    void SetLocalScore(int score);  // 점수 관련
    bool GetRemotePosition(Protocol::PlayerPosition& position);
    void Disconnect();

private:
    HWND window_ = nullptr; // 수신 알림 대상 창
    SOCKET sock = INVALID_SOCKET;    //소켓 번호
    bool winsockStarted = false;    //WSAStartup 성공 여부

    void ReceiveLoop(std::stop_token stop, SOCKET connectedSocket); // 대기방 패킷 수신 함수
    std::jthread receiveThread;                                    // 클라이언트 수신 스레드
    void SendLoop(std::stop_token stop, SOCKET connectedSocket);
    std::jthread sendThread;
    std::mutex stateMutex;
    std::condition_variable_any sendReady;
    bool startRequested = false;
    bool positionEnabled = false;
    float localX = 0.0f;
    float localY = 0.0f;
    Protocol::PlayerPosition remotePosition{};
    bool hasRemotePosition = false;

    int localScore = -1;        // 점수 0점인 경우 보내기 위해, -1 로 설정
    bool scorePending = false;


};
