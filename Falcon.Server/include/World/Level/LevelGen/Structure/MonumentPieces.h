#pragma once

#include "World/Level/LevelGen/Structure/StructurePiece.h"

#include <array>
#include <cstdint>
#include <random>
#include <vector>

class Level;
class LevelChunk;

// the ocean monument: a building whose rooms sit on a 5x5x3 grid, opened onto each other at random and fitted with
// rooms of one, two or four cells, two wings and a penthouse
namespace MonumentPieces {
    // faces as the game numbers them
    constexpr int32_t DOWN = 0;
    constexpr int32_t UP = 1;
    constexpr int32_t NORTH = 2;
    constexpr int32_t SOUTH = 3;
    constexpr int32_t WEST = 4;
    constexpr int32_t EAST = 5;

    constexpr int32_t NO_ROOM = -1;

    struct Room {
        // 0 to 74 on the grid, x + 5 z + 25 y; the wings' and the penthouse's doors are 1001 to 1003
        int32_t mIndex = 0;
        // slots in the layout's rooms
        std::array<int32_t, 6> mConnections = {NO_ROOM, NO_ROOM, NO_ROOM, NO_ROOM, NO_ROOM, NO_ROOM};
        std::array<bool, 6> mOpenings = {};
        bool mClaimed = false;
        bool mSource = false;
        int32_t mScanIndex = 0;
    };

    enum class Type : uint8_t {
        Building,
        Entry,
        Core,
        Simple,
        SimpleTop,
        DoubleX,
        DoubleY,
        DoubleZ,
        DoubleXY,
        DoubleYZ,
        Wing,
        Penthouse
    };

    struct Piece : public StructurePiece {
        Piece() = default;

        Piece(Type type, const Box &box, int32_t orientation) : StructurePiece(box, orientation), mType(type) {}

        Type mType = Type::Building;
        // the slot of the room the piece grew from
        int32_t mRoom = NO_ROOM;
        // the look of a simple room or a wing
        int32_t mDesign = 0;
        // the wing built second, which holds its elder guardian at the other end
        bool mSecondWing = false;
    };

    struct Layout {
        std::vector<Room> mRooms;
        // the building, then its rooms, the two wings and the penthouse, in the order the game builds them
        std::vector<Piece> mPieces;
    };

    // the monument started in the chunk; reads only the seed
    Layout plan(int64_t seed, int32_t chunkX, int32_t chunkZ, int32_t seaLevel);

    // lays every piece meeting the chunk; the random draws the simple rooms' centre pillars
    void postProcess(const Layout &layout, Level &level, LevelChunk &chunk, std::mt19937 &random, int32_t seaLevel);
}
