#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32")

#include <stop_token>
#include <thread>
#include <functional> // std::ref() 참조로 전달

#include "GameCommandQueue.h"

#define SERVERPORT 9000
#define BUFSIZE    1024

// 스레드
void ReceiveLoop(std::stop_token stop, SOCKET listenSocket, GameCommandQueue& queue);
void GameLoop(std::stop_token stop, GameCommandQueue& queue);
void SendLoop(std::stop_token stop);

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

    GameCommandQueue queue; // 두 스레드가 사용할 큐

	// 스레드
	std::jthread receiveThread(ReceiveLoop, listen_sock, std::ref(queue));
	std::jthread gameThread(GameLoop, std::ref(queue));
	//std::jthread sendThread(SendLoop);

	std::cout << "Press Enter to stop the server.\n";
	std::cin.get();

	// 세 스레드에 모두 종료를 요청한 뒤 종료를 기다려.
	receiveThread.request_stop();
	gameThread.request_stop();
	//sendThread.request_stop();

	// 소켓 닫기
	closesocket(listen_sock);

	// 윈속 종료
	WSACleanup();
}

void ReceiveLoop(std::stop_token stop, SOCKET listenSocket, GameCommandQueue& queue)
{
    // 플레이어 둘
    SOCKET clients[2] = { INVALID_SOCKET, INVALID_SOCKET }; // 클라이언트 통신 소켓
    char buffer[BUFSIZE];

    while (!stop.stop_requested()) // 종료 요청이 오기 전까지 계속 반복
    {
        // select()가 목록을 바꾸므로 매번 다시 구성
        fd_set readable;                // select()에 넘길 소켓 목록
        FD_ZERO(&readable);             // 목록 비우기
        FD_SET(listenSocket, &readable);// 접속 대기 소켓을 목록에 추가

        for (SOCKET client : clients)   // 소켓을 하나씩 꺼내서 목록에 추가
        {
            if (client != INVALID_SOCKET)
                FD_SET(client, &readable);
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
            SOCKET& client = clients[slot]; // 배열의 원래 자리를 참조

            if (client == INVALID_SOCKET || !FD_ISSET(client, &readable)) // 소켓이 빔 or select()가 돌려준 목록에 소켓이 없는지
                continue;

            int received = recv(client, buffer, sizeof(buffer), 0); // 수신

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

            closesocket(client);                              // 끊긴 소켓 닫기
            client = INVALID_SOCKET;                          // 배열에서 그 자리 비우기
            queue.Push({ CommandType::Disconnected, slot });    // 어느 자리가 비었는지 알림
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

            int slot = clients[0] == INVALID_SOCKET ? 0 : 1; // 소켓이 비어있으면 0, 차 있으면 1
            if (clients[slot] != INVALID_SOCKET)
            {
                closesocket(client); // 두 자리가 모두 차 있음
                continue;
            }

            clients[slot] = client; // 새 소켓 저장
            queue.Push({ CommandType::Connected, slot });    // 어느 자리에 연결됐는지 알림
            std::printf("Client 연결, 플레이어 %d\n", slot + 1);
        }
    }

    // 소켓 닫기
    for (SOCKET client : clients)
    {
        if (client != INVALID_SOCKET)
            closesocket(client);
    }
}

void GameLoop(std::stop_token stop, GameCommandQueue& queue)
{
    GameCommand command{}; // 큐에서 꺼낸 명령

    while (queue.WaitPop(stop, command)) // 명령을 받을 때마다 반복
    {
        switch (command.type)           // 접속과 이탈을 구분
        {
        case CommandType::Connected:
            std::printf("[GameLoop] Slot %d 연결\n", command.slot + 1);
            break;                     // switch를 빠져나온 뒤 다음 명령을 기다려.

        case CommandType::Disconnected:
            std::printf("[GameLoop] Slot %d 접속 종료\n", command.slot + 1);
            break;
        }
    }
}