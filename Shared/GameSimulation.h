#pragma once
#include "GameState.h"
#include "InputCommand.h"
#include <vector>

struct GameBounds
{
    int left;
    int top;
    int right;
    int bottom;
};

enum class GameEvent
{
    BossHit, PlayerHit, Revived, Healed, SpeedStarted,
    Poisoned, FogStarted, MeteorStarted, SpeedEnded, FogEnded
};

enum class RoundOutcome { Running, Won, Lost };

struct WorldStep
{
    RoundOutcome outcome = RoundOutcome::Running;
    std::vector<GameEvent> events;
};

void ResetGame(GameState& state, const GameBounds& bounds);
bool UpdatePlayer(GameState& state, const GameBounds& bounds,
    const InputCommand& command);
WorldStep UpdateWorld(GameState& state, const GameBounds& bounds);
std::vector<GameEvent> UpdateGameClock(GameState& state, const GameBounds& bounds);
