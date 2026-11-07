#pragma once

#include <climits>
#include <cstdint>
#include <vector>

// Density the game adds around jigsaw structure pieces before the aquifer runs, so that the terrain
// meets the pieces (sub_145C33980 gathers the pieces, sub_145C34670 samples them).
class TerrainBeard {
public:
    enum class Adaptation : uint8_t {
        None = 0,
        Bury = 1,
        BeardThin = 2,
        BeardBox = 3,
        Encapsulate = 4,
    };

    struct Piece {
        int32_t mMinX;
        int32_t mMinY;
        int32_t mMinZ;
        int32_t mMaxX;
        int32_t mMaxY;
        int32_t mMaxZ;
        Adaptation mAdaptation;
        int32_t mGroundLevelDelta;
    };

    struct Junction {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
    };

    // pieces are kept when their box comes within 12 blocks of the chunk; junctions only inside the
    // chunk and the 12 blocks past its high side, as the game filters them
    void addPiece(int32_t chunkX, int32_t chunkZ, const Piece &piece);
    void addJunction(int32_t chunkX, int32_t chunkZ, const Junction &junction);

    bool empty() const {
        return mPieces.empty() && mJunctions.empty();
    }

    // lowest and highest y a cell must reach to be adjusted
    int32_t getMinY() const {
        return mMinY;
    }

    int32_t getMaxY() const {
        return mMaxY;
    }

    double compute(int32_t x, int32_t y, int32_t z) const;

private:
    std::vector<Piece> mPieces;
    std::vector<Junction> mJunctions;
    int32_t mMinY = INT32_MAX;
    int32_t mMaxY = INT32_MIN;
};
