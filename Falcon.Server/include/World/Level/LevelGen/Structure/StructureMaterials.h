#pragma once

#include "World/Level/Block/BlockState.h"

// the game's block materials, as the structure pieces test them
namespace StructureMaterials {
    // the materials flagged liquid: water, lava and the bubble column
    bool isLiquid(const BlockState &state);

    // the material of the small plants: grasses, flowers, saplings, crops, vines
    bool isPlant(const BlockState &state);

    // LevelChunk::getAboveTopSolidBlock (sub_1438650A0, 26.60) going down: stops on leaves only when they count,
    // on water when it counts, else on the solid material flag
    bool stopsTopSolidSearch(const BlockState &state, bool water, bool leaves);
}
