#pragma once

class ChunkGenerator;
class Level;
class LevelChunk;

// the temple start of the swamps: one piece built by code, whole inside its start chunk
class SwampHutStructure {
public:
    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
