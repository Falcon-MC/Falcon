#pragma once

#include "World/Level/LevelGen/Structure/StructurePiece.h"

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;

/**
 * The pieces of a stronghold, laid from a staircase in its start chunk: each piece opens doors its children are
 * drawn for, one weighted pick among the piece kinds, a filler corridor when nothing fits.
 */
class StrongholdStructure {
public:
    enum class Type : uint8_t {
        Start,
        Straight,
        PrisonHall,
        LeftTurn,
        RightTurn,
        RoomCrossing,
        StraightStairsDown,
        StairsDown,
        FiveCrossing,
        ChestCorridor,
        Library,
        PortalRoom,
        FillerCorridor
    };

    // the door a piece was entered through
    enum class Door : uint8_t { Opening, WoodDoor, Grates, IronDoor };

    struct Piece : StructurePiece {
        Piece() = default;

        Piece(Type type, const Box &box, int32_t orientation, int32_t depth)
                : StructurePiece(box, orientation), mType(type), mDepth(depth) {}

        Type mType = Type::Start;
        int32_t mDepth = 0;
        Door mEntryDoor = Door::Opening;
        // straight: left and right exits; five crossing: low left, high left, high right, low right; library: tall
        bool mFlags[4] = {false, false, false, false};
        // room crossing: its furnishing; filler corridor: its length
        int32_t mValue = 0;
    };

    struct Layout {
        std::vector<Piece> mPieces;
        StructurePiece::Box mBox;
    };

    static Layout plan(int64_t seed, int32_t chunkX, int32_t chunkZ);

    // kept for the seed and the start chunk: every chunk the stronghold reaches reads the same layout
    static std::shared_ptr<const Layout> cachedPlan(int64_t seed, int32_t chunkX, int32_t chunkZ);

    static const char *nameOf(Type type);

    // every stronghold piece reaching the chunk, built with the chunk's post process random
    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);

    // the part of the piece inside the chunk; the random is drawn for the whole piece
    static void postProcess(const Piece &piece, Level &level, LevelChunk &chunk, std::mt19937 &random);

    // the twelve frames of a portal room, an eye in those flagged, and the portal when all have one
    static void placePortal(const Piece &room, Level &level, LevelChunk &chunk, const std::array<bool, 12> &eyes);
};
