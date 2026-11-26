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

    // a structure's terrain_adaptation, for each of its rigid pieces; BeardAndShaver: the ancient city's, which fills
    // under the piece and empties it above its ground
    enum class Adaptation : uint8_t { Bury, BeardThin, Encapsulate, BeardAndShaver };

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

    // a village piece: the game keeps it for the chunks within 12 blocks of its box, then beards the box grown by 2
    // on each side, in single precision and with no junctions
    void addVillagePiece(int32_t chunkX, int32_t chunkZ, const Box &box, int32_t groundY);

    bool empty() const {
        return mPieces.empty() && mJunctions.empty() && mBuried.empty() && mVillagePieces.empty();
    }

    // lowest and highest y a cell must reach to be adjusted
    int32_t getMinY() const {
        return mMinY;
    }

    int32_t getMaxY() const {
        return mMaxY;
    }

    double compute(int32_t x, int32_t y, int32_t z) const;

    // what the village pieces and then the buried boxes add to a density the other pieces already moved; the
    // buried boxes count only where the sum would otherwise be empty
    float adjust(int32_t x, int32_t y, int32_t z, float density) const;

private:
    struct Junction {
        int32_t x;
        int32_t y;
        int32_t z;
    };

    // the grown box
    struct VillagePiece {
        Box mBox;
        int32_t mGroundY;
    };

    std::vector<Piece> mPieces;
    std::vector<Junction> mJunctions;
    std::vector<Box> mBuried;
    std::vector<VillagePiece> mVillagePieces;
    int32_t mMinY = INT32_MAX;
    int32_t mMaxY = INT32_MIN;
};
