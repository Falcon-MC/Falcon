#pragma once

class ChunkGenerator;
class Level;
class LevelChunk;

class BuriedTreasureStructure {
public:
    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
