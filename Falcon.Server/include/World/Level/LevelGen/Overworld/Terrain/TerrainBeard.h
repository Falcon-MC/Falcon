#pragma once

#include <climits>
#include <cstdint>
#include <vector>

// Density the game adds around jigsaw structure pieces before the aquifer runs, so that the terrain
// meets the pieces (sub_145C33980 gathers the pieces, sub_145C34670 samples them).
// TODO: junctions and the bury, beard_box and encapsulate adaptations; villages only use beard_thin
class TerrainBeard {
public:
    // a beard_thin piece
    struct Piece {
        int32_t mMinX;
        int32_t mMinY;
        int32_t mMinZ;
        int32_t mMaxX;
        int32_t mMaxY;
        int32_t mMaxZ;
        int32_t mGroundLevelDelta;
    };

    // pieces are kept when their box comes within 12 blocks of the chunk
    void addPiece(int32_t chunkX, int32_t chunkZ, const Piece &piece);

    bool empty() const {
        return mPieces.empty();
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
    int32_t mMinY = INT32_MAX;
    int32_t mMaxY = INT32_MIN;
};
