#pragma once

#include "World/Level/Block/BlockState.h"

// the precipitation interaction the game gives a block (sub_145C617E0); it sets one for about 190 vanilla blocks
// (sub_14CE9AD30) and air (sub_1431FA960), every other block stops rain
namespace BlockPrecipitation {
    // interaction none or snowlogging
    bool letsRainThrough(const BlockState &state);

    // interaction snowlogging: a snow layer may be placed in its position
    bool isSnowloggable(const BlockState &state);
}
