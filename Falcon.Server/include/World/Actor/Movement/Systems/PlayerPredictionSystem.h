#pragma once

#include "Core/Math/Vector3f.h"

struct PlayerMovementContext;

class PlayerPredictionSystem {
public:
    static void tick(PlayerMovementContext &context);

    // feet position; the client rewinds to this input's tick and replays from there
    static void correct(PlayerMovementContext &context, const Vector3f &position, const Vector3f &velocity,
                        bool onGround);
};
