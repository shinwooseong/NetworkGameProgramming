#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32")

#include <stop_token>
#include <thread>
#include <functional> // std::ref() 참조로 전달


#include "GameCommandQueue.h"
#include "SendCommandQueue.h"

#define SERVERPORT 9000
#define BUFSIZE    1024

// 스레드
void ReceiveLoop(std::stop_token stop, SOCKET listenSocket, GameCommandQueue& queue);
void GameLoop(std::stop_token stop, GameCommandQueue& gameQueue, SendCommandQueue& sendQueue);
void SendLoop(std::stop_token stop, SendCommandQueue& sendQueue);

int main()
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;
	// 소켓 생성
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) exit(1);
	
	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) exit(1);

	// listen()
	retval = listen(listen_sock, 5);
	if (retval == SOCKET_ERROR) exit(1);

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	int addrlen;
	HANDLE hThread;

    GameCommandQueue gameQueue; // 두 스레드가 사용할 큐
    SendCommandQueue sendQueue; // 게임 연산에서 송신으로 전달할 명령 큐

	// 스레드
	std::jthread receiveThread(ReceiveLoop, listen_sock, std::ref(gameQueue));
	std::jthread gameThread(GameLoop, std::ref(gameQueue), std::ref(sendQueue));
	std::jthread sendThread(SendLoop, std::ref(sendQueue));

	std::cout << "Press Enter to stop the server.\n";
	std::cin.get();

	// 세 스레드에 모두 종료를 요청한 뒤 종료를 기다려.
	receiveThread.request_stop();
	gameThread.request_stop();
	sendThread.request_stop();

	// 소켓 닫기
	closesocket(listen_sock);

	// 윈속 종료
	WSACleanup();
}

void QueueLobbyState(const std::array<std::shared_ptr<ClientSession>, 2>& sessions, int hostPlayerNumber, SendCommandQueue& sendQueue)
{
    for (int i = 0; i < 2; ++i) // 두 슬롯 순회
    {
        if (!sessions[i])
            continue; // 빈 슬롯 제외

        Protocol::LobbyPacket packet{
            Protocol::LobbyStateType,                           // 메시지 종류
            static_cast<std::uint8_t>(i + 1),                   // 수신자의 플레이어 번호
            static_cast<std::uint8_t>(hostPlayerNumber),        // 현재 방장 번호
            static_cast<std::uint8_t>(sessions[0] != nullptr),  // 1번 접속 여부
            static_cast<std::uint8_t>(sessions[1] != nullptr)   // 2번 접속 여부
        };

        sendQueue.Push({ sessions[i], packet }); // 대상 연결과 패킷의 송신 큐 등록
    }
}

void ReceiveLoop(std::stop_token stop, SOCKET listenSocket, GameCommandQueue& queue)
{
    // 플레이어 둘
    //SOCKET clients[2] = { INVALID_SOCKET, INVALID_SOCKET }; // 클라이언트 통신 소켓
    std::array<std::shared_ptr<ClientSession>, 2> clients{}; // 수신 스레드의 연결 목록
    char buffer[BUFSIZE];

    while (!stop.stop_requested()) // 종료 요청이 오기 전까지 계속 반복
    {
        // select()가 목록을 바꾸므로 매번 다시 구성
        fd_set readable;                // select()에 넘길 소켓 목록
        FD_ZERO(&readable);             // 목록 비우기
        FD_SET(listenSocket, &readable);// 접속 대기 소켓을 목록에 추가

        for (const auto& session : clients)
        {
            if (session)
                FD_SET(session->socket, &readable); // 접속 세션의 소켓 등록
        }

        timeval timeout{ 0, 100000 }; // 최대 100ms 대기
        int result = select(0, &readable, nullptr, nullptr, &timeout); // 처리할 소켓이 생길 때까지 대기

        if (result == SOCKET_ERROR) // select 실패
        {
            std::printf("select 실패: %d\n", WSAGetLastError());
            break;
        }
        if (result == 0 || stop.stop_requested()) // 준비된 소켓 없음 or 종료 요청 받음
            continue;

        // 1. 기존 클라이언트가 보낸 데이터나 연결 종료를 처리
        for (int slot = 0; slot < 2; ++slot)
        {
            auto& session = clients[slot]; // 배열에 저장된 세션 참조

            if (!session || !FD_ISSET(session->socket, &readable)) // 빈자리 또는 읽기 대기 소켓 제외
                continue;
            const int received = recv(session->socket, buffer, sizeof(buffer), 0); // 수신

            if (received > 0)
            {
                std::printf("%d 바이트 받음\n", received);
                // 다음 단계: 받은 바이트를 연결별 수신 버퍼에 추가
                continue;
            }

            if (received == SOCKET_ERROR) // 수신 실패
            {
                const int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) // 읽을 데이터 없음
                    continue;

                std::printf("recv 실패: %d\n", error);
            }

            closesocket(session->socket);              // 끊긴 소켓 닫기
            queue.Push({ CommandType::Disconnected, slot, session });  // 어느 자리가 비었는지 알림
            session.reset();                                  // 수신 목록의 슬롯 해제
        }

        // 2. 대기 중인 새 연결을 받아 빈 자리에 보관해.
        if (FD_ISSET(listenSocket, &readable)) // 접속 대기 소켓이 있는지 확인
        {
            SOCKET client = accept(listenSocket, nullptr, nullptr);
            if (client == INVALID_SOCKET) // accept() 실패했는지
            {
                const int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) // 수락할 연결이 없음
                    std::printf("accept 실패: %d\n", error);
                continue;
            }

            const int slot = !clients[0] ? 0 : 1; // 소켓이 비어있으면 0, 차 있으면 1
            if (clients[slot])
            {
                closesocket(client); // 두 자리가 모두 차 있음
                continue;
            }

            clients[slot] = std::make_shared<ClientSession>(client); // 새 소켓 저장
            queue.Push({ CommandType::Connected, slot, clients[slot]});    // 어느 자리에 연결됐는지 알림
            std::printf("Client 연결, 플레이어 %d\n", slot + 1);
        }
    }

    // 소켓 닫기
    for (auto& session : clients)
    {
        if (!session)
            continue; // 빈 슬롯 제외

        shutdown(session->socket, SD_BOTH); // 연결의 송수신 중단
        session.reset();                    // 수신 목록의 세션 참조 해제
    }
}

void GameLoop(std::stop_token stop, GameCommandQueue& gameQueue, SendCommandQueue& sendQueue)
{
    std::array<std::shared_ptr<ClientSession>, 2> sessions{};   // 대기방 슬롯 두 자리
    int hostPlayerNumber = 0;                                   // 방장 미지정
    GameCommand command{}; // 큐에서 꺼낸 명령

    while (gameQueue.WaitPop(stop, command)) // 명령을 받을 때마다 반복
    {
        const int slot = command.slot;        // 배열 인덱스 0 또는 1
        const int playerNumber = slot + 1;   // 화면 표시 번호 1 또는 2

        if (slot < 0 || slot >= static_cast<int>(sessions.size()) || !command.session)
        {
            continue; // 유효하지 않은 슬롯 또는 빈 세션 제외
        }

        switch (command.type)           // 접속과 이탈을 구분
        {
        case CommandType::Connected:
            if (sessions[slot])
                continue; // 이미 사용 중인 슬롯 제외

            sessions[slot] = command.session; // 대기방 슬롯에 새 세션 등록

            if (hostPlayerNumber == 0)
                hostPlayerNumber = playerNumber; // 방장이 없는 경우 새 방장 지정

            std::printf("[GameLoop] Slot %d 연결\n", command.slot + 1);
            break;                     // switch를 빠져나온 뒤 다음 명령을 기다려.

        case CommandType::Disconnected:
            if (sessions[slot] != command.session)
                continue; // 현재 슬롯과 일치하지 않는 이탈 명령 제외

            sessions[slot].reset(); // 이탈한 슬롯의 세션 참조 해제

            if (hostPlayerNumber == playerNumber)
            {
                const int otherSlot = 1 - slot; // 상대 슬롯 인덱스 계산
                hostPlayerNumber = sessions[otherSlot] ? otherSlot + 1 : 0; // 방장 이전 또는 해제
            }
            std::printf("[GameLoop] Slot %d 접속 종료\n", command.slot + 1);
            break;
        }

        std::printf("[Lobby] P1:%c P2:%c 방장: P%d\n",
            sessions[0] ? 'O' : 'X', sessions[1] ? 'O' : 'X', hostPlayerNumber);

        QueueLobbyState(sessions, hostPlayerNumber, sendQueue); // 변경된 대기방 상태의 송신 예약
    }
}

void SendLoop(std::stop_token stop, SendCommandQueue& sendQueue)
{
    //using Clock = std::chrono::steady_clock; // 경과시간 측정용 시계
    SendCommand command{};                  // 송신 큐에서 꺼낼 명령

    while (!stop.stop_requested() && sendQueue.WaitPop(stop, command))
    {
        const SOCKET socket = command.target->socket; // 대상 세션의 소켓
        const char* bytes =
            reinterpret_cast<const char*>(command.packet.data()); // 패킷 시작 주소
        const int length = static_cast<int>(command.packet.size()); // 전체 바이트 수
        int sent = 0;                                               // 송신 완료 바이트 수
        //const auto deadline = Clock::now() + std::chrono::seconds(1); // 송신 제한 시각

        //while (sent < length && !stop.stop_requested() && Clock::now() < deadline)
        while (sent < length && !stop.stop_requested())
        {
            const int result = send(socket, bytes + sent, length - sent, 0); // 남은 바이트 송신

            if (result > 0)
            {
                sent += result; // 송신 완료 위치 갱신
                continue;
            }

            if (result != SOCKET_ERROR)
                break; // 송신 진행 불가

            const int error = WSAGetLastError(); // 송신 오류 번호 조회
            if (error != WSAEWOULDBLOCK)
                break; // 재시도할 수 없는 오류

            fd_set writable;
            FD_ZERO(&writable);        // 쓰기 감시 목록 초기화
            FD_SET(socket, &writable); // 대상 소켓 등록

            timeval timeout{ 0, 50000 }; // 최대 50ms 대기
            if (select(0, nullptr, &writable, nullptr, &timeout) == SOCKET_ERROR)
                break; // 쓰기 감시 실패
        }

        if (sent != length)
            shutdown(socket, SD_BOTH); // 미완료 패킷이 있는 연결의 송수신 중단

        command = {}; // 처리한 명령의 세션 참조 해제
    }
}