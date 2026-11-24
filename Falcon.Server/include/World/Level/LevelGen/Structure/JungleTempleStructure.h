#pragma once

class ChunkGenerator;
class Level;
class LevelChunk;

// the temple start of the jungles: one 12 by 15 piece built by code, whole inside its start chunk
class JungleTempleStructure {
public:
    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
