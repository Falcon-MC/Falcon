#pragma once

class ChunkGenerator;
class Level;
class LevelChunk;

namespace NetherStructures {
    void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
}
