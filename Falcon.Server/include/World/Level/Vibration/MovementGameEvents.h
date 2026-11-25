#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

class Actor;
class ServerNetworkHandler;

// what an actor's movement has added up to: a step, a swim stroke or a flap is due once it passes mNextStep
struct MovementGameEventState {
    bool mStarted = false;
    Vector3f mLast;
    float mDistance = 0.0f;
    float mNextStep = 0.0f;
    int64_t mNextFlapTick = 0;
};

// the game's movement tracking: entity_move on land, swim in water, flap for the fliers, once a tick
namespace MovementGameEvents {
    // each step adds the move times this, the height only while the actor can climb or swim
    constexpr float DISTANCE_SCALE = 3.0f;
    constexpr int64_t FLAP_INTERVAL_TICKS = 10;

    void track(ServerNetworkHandler &owner, Actor &actor);
}
