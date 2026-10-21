#pragma once

#include <cstdint>
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"

struct PlayerLiquidContactComponent {
    LiquidContact liquid;
    bool lavaTravel = false;
    bool waterTravel = false;
};
