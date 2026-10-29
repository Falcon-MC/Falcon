#pragma once

#include <cstdint>
struct MovementAbilitiesComponent {
    int32_t gameType = 0;
    bool spectator = false;
    bool flying = false;
    bool gliding = false;
    bool swimming = false;
    bool sprinting = false;
};
