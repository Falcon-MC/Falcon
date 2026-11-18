#pragma once

class ChunkGenerator;
class Level;
class LevelChunk;

namespace MansionStructure {
    void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
}
