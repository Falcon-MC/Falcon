#pragma once

class ChunkGenerator;
class Level;
class LevelChunk;

// the temple start of the deserts: one 21 by 21 piece built by code, over up to four chunks
class DesertPyramidStructure {
public:
    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
