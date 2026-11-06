#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <functional>

/**
 * What a fence or bars of a structure connect to. The files keep the shapes of the editor, so the game works them
 * out from whatever stands next to the block once it is down, terrain included.
 */
class StructureConnections {
public:
    // the block at a world position, terrain and already placed structure blocks included
    using Lookup = std::function<BlockState(int32_t, int32_t, int32_t)>;

    // true when a post connects to the neighbour found by stepping sideX, sideZ from it
    static bool connects(const BlockState &neighbour, int32_t sideX, int32_t sideZ);

    // the block with its four connections set from its neighbours, given by east, north, south and west
    static BlockState connected(const BlockState &post, bool east, bool north, bool south, bool west);

    // a fence or thin fence with its connections, or a stair with its corner, worked out from the neighbours at
    // x, y, z; any other block is returned unchanged
    static BlockState shaped(const BlockState &block, int32_t x, int32_t y, int32_t z, const Lookup &at);
};
