#include "World/Level/LevelGen/Feature/Tree/TreeBlockFace.h"

const TreeBlockFace TreeBlockFaces::HORIZONTAL_PLANE[4] = {
        TreeBlockFace::NORTH,
        TreeBlockFace::EAST,
        TreeBlockFace::SOUTH,
        TreeBlockFace::WEST
};

const TreeBlockFace TreeBlockFaces::HORIZONTALS[4] = {
        TreeBlockFace::SOUTH,
        TreeBlockFace::WEST,
        TreeBlockFace::NORTH,
        TreeBlockFace::EAST
};

int32_t TreeBlockFaces::getXOffset(TreeBlockFace face) {
    switch (face) {
        case TreeBlockFace::WEST:
            return -1;
        case TreeBlockFace::EAST:
            return 1;
        default:
            return 0;
    }
}

int32_t TreeBlockFaces::getYOffset(TreeBlockFace face) {
    switch (face) {
        case TreeBlockFace::DOWN:
            return -1;
        case TreeBlockFace::UP:
            return 1;
        default:
            return 0;
    }
}

int32_t TreeBlockFaces::getZOffset(TreeBlockFace face) {
    switch (face) {
        case TreeBlockFace::NORTH:
            return -1;
        case TreeBlockFace::SOUTH:
            return 1;
        default:
            return 0;
    }
}

TreeBlockFace TreeBlockFaces::getOpposite(TreeBlockFace face) {
    switch (face) {
        case TreeBlockFace::DOWN:
            return TreeBlockFace::UP;
        case TreeBlockFace::UP:
            return TreeBlockFace::DOWN;
        case TreeBlockFace::NORTH:
            return TreeBlockFace::SOUTH;
        case TreeBlockFace::SOUTH:
            return TreeBlockFace::NORTH;
        case TreeBlockFace::WEST:
            return TreeBlockFace::EAST;
        default:
            return TreeBlockFace::WEST;
    }
}

int32_t TreeBlockFaces::getHorizontalIndex(TreeBlockFace face) {
    switch (face) {
        case TreeBlockFace::SOUTH:
            return 0;
        case TreeBlockFace::WEST:
            return 1;
        case TreeBlockFace::NORTH:
            return 2;
        case TreeBlockFace::EAST:
            return 3;
        default:
            return -1;
    }
}
