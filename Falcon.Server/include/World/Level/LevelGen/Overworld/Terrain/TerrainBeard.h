#pragma once

#include <climits>
#include <cstdint>
#include <vector>

// Density the game adds around jigsaw structure pieces and buried strongholds before the aquifer runs, so that
// the terrain meets the pieces.
// TODO: beard_box; no structure of the game uses it
class TerrainBeard {
public:
    struct Box {
        int32_t mMinX;
        int32_t mMinY;
        int32_t mMinZ;
        int32_t mMaxX;
        int32_t mMaxY;
        int32_t mMaxZ;
    };

    // a structure's terrain_adaptation, for each of its rigid pieces
    enum class Adaptation : uint8_t { Bury, BeardThin, Encapsulate };

    struct Piece {
        int32_t mMinX;
        int32_t mMinY;
        int32_t mMinZ;
        int32_t mMaxX;
        int32_t mMaxY;
        int32_t mMaxZ;
        int32_t mGroundLevelDelta;
        Adaptation mAdaptation = Adaptation::BeardThin;
    };

    // pieces are kept when their box comes within 12 blocks of the chunk
    void addPiece(int32_t chunkX, int32_t chunkZ, const Piece &piece);

    // where a rigid piece meets a piece that follows the ground: the x, ground y and z of its jigsaw
    void addJunction(int32_t chunkX, int32_t chunkZ, int32_t x, int32_t y, int32_t z);

    // a box the caves around are filled near, as under a stronghold; kept when within 12 blocks of the chunk
    void addBuriedBox(int32_t chunkX, int32_t chunkZ, const Box &box);

    bool empty() const {
        return mPieces.empty() && mJunctions.empty() && mBuried.empty();
    }

    // lowest and highest y a cell must reach to be adjusted
    int32_t getMinY() const {
        return mMinY;
    }

    int32_t getMaxY() const {
        return mMaxY;
    }

    double compute(int32_t x, int32_t y, int32_t z) const;

    // the density the buried boxes add, only where the terrain would otherwise be empty
    float buried(int32_t x, int32_t y, int32_t z) const;

private:
    struct Junction {
        int32_t x;
        int32_t y;
        int32_t z;
    };

    std::vector<Piece> mPieces;
    std::vector<Junction> mJunctions;
    std::vector<Box> mBuried;
    int32_t mMinY = INT32_MAX;
    int32_t mMaxY = INT32_MIN;
};
