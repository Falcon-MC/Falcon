#pragma once

#include "World/Level/LevelGen/Structure/MonumentPieces.h"

#include <cstdint>
#include <memory>

class ChunkGenerator;
class Level;
class LevelChunk;

// the monuments reaching a chunk: planned once from their start chunk, then laid a chunk at a time
class MonumentStructure {
public:
    static const char *const NAME;

    // nullptr when the chunk starts no monument
    static std::shared_ptr<const MonumentPieces::Layout> plan(const ChunkGenerator &generator, int32_t startX,
                                                              int32_t startZ);

    // lays the monuments reaching the chunk, spawns their guardians and records them in its structure bounds
    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
