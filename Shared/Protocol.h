#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace Protocol
{
    constexpr std::uint8_t LobbyStateType = 1; // 대기방 상태 메시지 번호
    constexpr std::uint8_t StartRequestType = 2; // 방장 시작 요청 번호
    constexpr std::uint8_t GameStartedType = 3; // 서버 시작 승인 번호
    using StartRequestPacket = std::array<std::uint8_t, 1>;
    using GameStartedPacket = std::array<std::uint8_t, 1>;

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

    using Packet = std::vector<std::uint8_t>; // 직렬화된 송신 바이트

    template <std::size_t Size>
    Packet SerializePacket(const std::array<std::uint8_t, Size>& packet)
    {
        return Packet(packet.begin(), packet.end()); // 필드 순서와 길이를 보존한 바이트 복사
    }
}
