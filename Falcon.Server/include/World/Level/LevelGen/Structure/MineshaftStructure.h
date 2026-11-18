#pragma once

#include "World/Level/LevelGen/Structure/StructurePiece.h"

#include <optional>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;

namespace MineshaftStructure {
    enum class Type {
        Room,
        Corridor,
        Crossing,
        Stairs
    };

    struct Piece : StructurePiece {
        Piece(Type type, const Box &box, int32_t direction, int32_t depth)
                : StructurePiece(box, direction), mType(type), mDepth(depth) {}

        Type mType;
        int32_t mDepth;
        bool mRails = false;
        bool mSpiders = false;
        std::vector<Box> mDoorways;
    };

    struct Plan {
        StructurePiece::Box mBounds;
        bool mMesa;
        std::vector<Piece> mPieces;
    };

    std::optional<Plan> plan(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ);
    void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
}
