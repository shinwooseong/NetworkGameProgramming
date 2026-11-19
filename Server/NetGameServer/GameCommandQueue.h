#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>

struct ClientSession;

enum class CommandType       // 큐에 넣을 명령의 종류를 정의
{
    Connected,              // 클라이언트가 접속했을 때
    Disconnected            // 클라이언트 연결이 끊겼을 때
};

struct GameCommand           // 큐에 들어가는 명령
{
    CommandType type;       // 접속인지 이탈인지 저장
    int slot;               // ReceiveLoop의 clients 배열 인덱스(0 또는 1)
    std::shared_ptr<ClientSession> session; // 연결 참조
};

class GameCommandQueue
{
public:
    void Push(GameCommand command);
    bool WaitPop(std::stop_token stop, GameCommand& command);

private:
    std::queue<GameCommand> commands;      
    std::mutex mutex;                            
    std::condition_variable_any ready;            
};

