#pragma once

#include <cstdint>

enum class TreeBlockFace : int32_t {
    DOWN = 0,
    UP = 1,
    NORTH = 2,
    SOUTH = 3,
    WEST = 4,
    EAST = 5
};

class TreeBlockFaces {
public:
    static const TreeBlockFace HORIZONTAL_PLANE[4];

    static const TreeBlockFace HORIZONTALS[4];

    static int32_t getXOffset(TreeBlockFace face);

    static int32_t getYOffset(TreeBlockFace face);

    static int32_t getZOffset(TreeBlockFace face);

    static TreeBlockFace getOpposite(TreeBlockFace face);

    static int32_t getHorizontalIndex(TreeBlockFace face);
};
