#pragma once

#include <cstdint>
#include "Protocol/Packets/PlayerAuthInputPacket.h"

struct MoveInputComponent {
    const PlayerAuthInputPacket *packet = nullptr;
    bool jumpHeld = false;
    bool pressingSneak = false;
    float sideways = 0.0f;
    float forward = 0.0f;

    const PlayerAuthInputPacket &getPacket() const {
        return *packet;
    }
};
