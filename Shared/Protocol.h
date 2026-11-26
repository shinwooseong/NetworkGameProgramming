#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <cmath>
#include <cstring>
#include <limits>

namespace Protocol
{
    constexpr std::uint8_t LobbyStateType = 1; // 대기방 상태 메시지 번호
    constexpr std::uint8_t StartRequestType = 2; // 방장 시작 요청 번호
    constexpr std::uint8_t GameStartedType = 3; // 서버 시작 승인 번호
    using StartRequestPacket = std::array<std::uint8_t, 1>;
    using GameStartedPacket = std::array<std::uint8_t, 1>;
    constexpr std::uint8_t PlayerPositionType = 4; // 플레이어 좌표 메시지 번호
    constexpr int PositionPacketSize = 10; // 타입 1바이트와 번호 1바이트 및 좌표 8바이트
    using PositionPacket = std::array<std::uint8_t, PositionPacketSize>;

    struct PlayerPosition
    {
        std::uint8_t playerNumber = 0;
        float x = 0.0f;
        float y = 0.0f;
    };

    inline void WriteFloat(std::uint8_t* destination, float value)
    {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        for (int i = 0; i < 4; ++i)
            destination[i] = static_cast<std::uint8_t>(bits >> (24 - i * 8));
    }

    inline float ReadFloat(const std::uint8_t* source)
    {
        std::uint32_t bits = 0;
        for (int i = 0; i < 4; ++i)
            bits = (bits << 8) | source[i];
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    inline PositionPacket MakePositionPacket(std::uint8_t playerNumber, float x, float y)
    {
        PositionPacket packet{};
        packet[0] = PlayerPositionType;
        packet[1] = playerNumber;
        WriteFloat(packet.data() + 2, x);
        WriteFloat(packet.data() + 6, y);
        return packet;
    }

    inline bool ReadPosition(const std::uint8_t* packet, PlayerPosition& position)
    {
        position = {packet[1], ReadFloat(packet + 2), ReadFloat(packet + 6)};
        return std::isfinite(position.x) && std::isfinite(position.y);
    }

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



    constexpr std::uint8_t PlayerScoreType = 5;

    // 타입 1바이트 + 플레이어 번호 1바이트 + 점수 4바이트
    constexpr int ScorePacketSize = 6;
    using ScorePacket = std::array<std::uint8_t, ScorePacketSize>;

    struct PlayerScore
    {
        std::uint8_t playerNumber = 0;
        int score = 0;
    };

    // 나눠 담기
    inline ScorePacket MakeScorePacket(std::uint8_t playerNumber, int score)
    {
        ScorePacket packet{};
        packet[0] = PlayerScoreType;
        packet[1] = playerNumber;

        // 점수를 정확히 32비트인 부호 없는 정수로 바꿔서, 비트연산.
        // 점수는 음수를 보내지 않음
        const auto value = static_cast<std::uint32_t>(score);
        for (int i = 0; i < 4; ++i) {
            packet[2 + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
        }


        return packet;
    }

    // 다시 합치기
    inline bool ReadScore(const std::uint8_t* packet, PlayerScore& score)
    {
        std::uint32_t value = 0;

        for (int i = 0; i < 4; ++i) {
            value = (value << 8) | packet[2 + i];
        }

        // 복원한 점수가 int에 저장 가능한 범위인지 확인
        if (value > static_cast<std::uint32_t>(
            (std::numeric_limits<int>::max)()))
        {
            return false;
        }

        score.playerNumber = packet[1];
        score.score = static_cast<int>(value);
        return true;
    }



}
