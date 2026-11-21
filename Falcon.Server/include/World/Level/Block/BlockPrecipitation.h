#pragma once

#include "World/Level/Block/BlockState.h"

// BlockData::mPrecipitation
namespace BlockPrecipitation {
    // interaction none or snowlogging
    bool letsRainThrough(const BlockState &state);

    // interaction snowlogging: a snow layer may be placed in its position
    bool isSnowloggable(const BlockState &state);
}
