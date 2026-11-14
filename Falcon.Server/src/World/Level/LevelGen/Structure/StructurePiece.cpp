#include "World/Level/LevelGen/Structure/StructurePiece.h"

#include <array>

StructurePiece::Box StructurePiece::Box::orient(int32_t x, int32_t y, int32_t z, int32_t offsetX, int32_t offsetY,
                                                int32_t offsetZ, int32_t sizeX, int32_t sizeY, int32_t sizeZ,
                                                int32_t orientation) {
    Box box;
    box.mMinY = y + offsetY;
    box.mMaxY = y + sizeY - 1 + offsetY;
    switch (orientation) {
        case NORTH:
            box.mMinX = x + offsetX;
            box.mMaxX = x + sizeX - 1 + offsetX;
            box.mMinZ = z - sizeZ + 1 + offsetZ;
            box.mMaxZ = z + offsetZ;
            break;
        case WEST:
            box.mMinX = x - sizeZ + 1 + offsetZ;
            box.mMaxX = x + offsetZ;
            box.mMinZ = z + offsetX;
            box.mMaxZ = z + sizeX - 1 + offsetX;
            break;
        case EAST:
            box.mMinX = x + offsetZ;
            box.mMaxX = x + sizeZ - 1 + offsetZ;
            box.mMinZ = z + offsetX;
            box.mMaxZ = z + sizeX - 1 + offsetX;
            break;
        default:
            box.mMinX = x + offsetX;
            box.mMaxX = x + sizeX - 1 + offsetX;
            box.mMinZ = z + offsetZ;
            box.mMaxZ = z + sizeZ - 1 + offsetZ;
            break;
    }
    return box;
}

int32_t StructurePiece::worldX(int32_t x, int32_t z) const {
    switch (mOrientation) {
        case SOUTH:
        case NORTH:
            return mBox.mMinX + x;
        case WEST:
            return mBox.mMaxX - z;
        case EAST:
            return mBox.mMinX + z;
        default:
            return x;
    }
}

int32_t StructurePiece::worldZ(int32_t x, int32_t z) const {
    switch (mOrientation) {
        case SOUTH:
            return mBox.mMinZ + z;
        case WEST:
        case EAST:
            return mBox.mMinZ + x;
        case NORTH:
            return mBox.mMaxZ - z;
        default:
            return z;
    }
}

int32_t StructurePiece::stairsDirection(int32_t direction) const {
    if (direction < 0 || direction > 3)
        return direction;

    static const std::array<int32_t, 4> WEST_TURN = {2, 3, 0, 1};
    static const std::array<int32_t, 4> EAST_TURN = {2, 3, 1, 0};
    switch (mOrientation) {
        case SOUTH:
            return direction == 2 ? 3 : direction == 3 ? 2 : direction;
        case WEST:
            return WEST_TURN[(size_t) direction];
        case EAST:
            return EAST_TURN[(size_t) direction];
        default:
            return direction;
    }
}
