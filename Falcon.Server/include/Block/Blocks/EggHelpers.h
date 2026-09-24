#pragma once

#include "Block/Block.h"

#include <string>

namespace EggHelpers {
    const char *const CRACKED_STATE = "cracked_state";

    bool isFullyCracked(const BlockState &state);

    BlockState cracked(const BlockState &state);

    float soundPitch();
}
