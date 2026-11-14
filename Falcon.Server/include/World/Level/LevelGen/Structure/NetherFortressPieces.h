#pragma once

#include "World/Level/LevelGen/Structure/StructurePiece.h"

#include <cstdint>
#include <random>
#include <vector>

// the pieces of a nether fortress, drawn in its start chunk from the region random: bridges from the start, castle
// corridors past the entrances, end fillers where nothing else fits
class NetherFortressPieces {
public:
    enum class Type {
        Start,
        BridgeStraight,
        BridgeCrossing,
        BridgeEndFiller,
        RoomCrossing,
        StairsRoom,
        MonsterThrone,
        CastleEntrance,
        CastleStalkRoom,
        CastleSmallCorridor,
        CastleSmallCorridorCrossing,
        CastleSmallCorridorRightTurn,
        CastleSmallCorridorLeftTurn,
        CastleCorridorStairs,
        CastleCorridorTBalcony
    };

    struct Piece : StructurePiece {
        Piece() = default;

        Piece(Type type, const Box &box, int32_t orientation, int32_t depth)
                : StructurePiece(box, orientation), mType(type), mDepth(depth) {}

        Type mType = Type::Start;
        int32_t mDepth = 0;
        // the end filler's own random
        int32_t mSelfSeed = 0;
        // the turns roll a chest when they are made
        bool mNeedsChest = false;
    };

    // furthest a piece's door may be from the start's corner, in x and z
    static constexpr int32_t MAX_DISTANCE = 112;

    static constexpr int32_t MAX_DEPTH = 30;

    // the pieces in the order the game made them, already moved to their final height; the random continues the
    // region random after the start test
    static std::vector<Piece> assemble(std::mt19937 &random, int32_t chunkX, int32_t chunkZ, int32_t seaLevel);

    static const char *getName(Type type);
};
