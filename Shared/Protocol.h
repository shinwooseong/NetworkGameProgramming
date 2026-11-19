#pragma once
#include <array>
#include <cstdint>

namespace Protocol
{
    constexpr std::uint8_t LobbyStateType = 1; // 대기방 상태 메시지 번호

    enum LobbyField : int
    {
        Type = 0,        // 메시지 종류
        MyPlayerNumber,  // 수신자의 플레이어 번호
        HostNumber,      // 현재 방장 번호
        Player1Occupied, // 1번 슬롯 접속 여부
        Player2Occupied, // 2번 슬롯 접속 여부
        LobbyPacketSize  // 전체 패킷 바이트 수
    };

    using LobbyPacket = std::array<std::uint8_t, LobbyPacketSize>;
}