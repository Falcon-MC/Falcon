#pragma once

#include <cstdint>
#include "Core/Math/Vector3f.h"
#include "World/Actor/Movement/PlayerMovementValidator.h"

/** Per-input prediction view; the validator persists on the player between replays. */
struct PlayerPredictionComponent {
    Vector3f *feetPosition = nullptr;
    PlayerMovementValidator *validator = nullptr;
    bool strict = false;
};
