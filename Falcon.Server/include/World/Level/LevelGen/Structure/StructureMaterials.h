#pragma once

#include "World/Level/Block/BlockState.h"

// the game's block materials, as the structure pieces test them
namespace StructureMaterials {
    // the materials flagged liquid: water, lava and the bubble column
    bool isLiquid(const BlockState &state);

    // the material of the small plants: grasses, flowers, saplings, crops, vines
    bool isPlant(const BlockState &state);

    // going down: stops on leaves only when they count,
    // on water when it counts, else on the solid material flag
    bool stopsTopSolidSearch(const BlockState &state, bool water, bool leaves);

    // material flags & 5 == 4 and property 0x40000
    bool isBuriedTreasureGround(const BlockState &state);
}
