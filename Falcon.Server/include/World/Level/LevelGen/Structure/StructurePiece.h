#pragma once

#include <cstdint>

// a piece of a structure built by code, turned by its orientation: 0 south, 1 west, 2 north, 3 east; local x and z
// map onto the box the way the game's pieces do, y is the box floor plus local y
class StructurePiece {
public:
    // inclusive on every face
    struct Box {
        int32_t mMinX = 0;
        int32_t mMinY = 0;
        int32_t mMinZ = 0;
        int32_t mMaxX = 0;
        int32_t mMaxY = 0;
        int32_t mMaxZ = 0;

        bool intersects(const Box &other) const {
            return mMaxX >= other.mMinX && mMinX <= other.mMaxX && mMaxZ >= other.mMinZ && mMinZ <= other.mMaxZ
                   && mMaxY >= other.mMinY && mMinY <= other.mMaxY;
        }

        void move(int32_t x, int32_t y, int32_t z) {
            mMinX += x;
            mMinY += y;
            mMinZ += z;
            mMaxX += x;
            mMaxY += y;
            mMaxZ += z;
        }

        bool operator==(const Box &other) const {
            return mMinX == other.mMinX && mMinY == other.mMinY && mMinZ == other.mMinZ && mMaxX == other.mMaxX
                   && mMaxY == other.mMaxY && mMaxZ == other.mMaxZ;
        }

        // the box of a piece of the given size grown from a door at (x, y, z), the offsets in the piece's frame
        static Box orient(int32_t x, int32_t y, int32_t z, int32_t offsetX, int32_t offsetY, int32_t offsetZ,
                          int32_t sizeX, int32_t sizeY, int32_t sizeZ, int32_t orientation);
    };

    static constexpr int32_t SOUTH = 0;
    static constexpr int32_t WEST = 1;
    static constexpr int32_t NORTH = 2;
    static constexpr int32_t EAST = 3;

    // an unturned piece: its y is taken as given, not raised to the box floor
    static constexpr int32_t NO_ORIENTATION = 255;

    StructurePiece() = default;

    StructurePiece(const Box &box, int32_t orientation) : mBox(box), mOrientation(orientation) {}

    const Box &getBox() const noexcept {
        return mBox;
    }

    Box &getBox() noexcept {
        return mBox;
    }

    int32_t getOrientation() const noexcept {
        return mOrientation;
    }

    int32_t worldX(int32_t x, int32_t z) const;

    int32_t worldY(int32_t y) const {
        return mOrientation == NO_ORIENTATION ? y : mBox.mMinY + y;
    }

    int32_t worldZ(int32_t x, int32_t z) const;

    // a stairs weirdo_direction turned with the piece
    int32_t stairsDirection(int32_t direction) const;

protected:
    Box mBox;
    int32_t mOrientation = NO_ORIENTATION;
};
