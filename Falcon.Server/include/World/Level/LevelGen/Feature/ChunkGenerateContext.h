#pragma once

#include <cstdint>

class Level;
class LevelChunk;

class ChunkGenerateContext {
public:
    ChunkGenerateContext(Level &level, LevelChunk &chunk) : mLevel(&level), mChunk(&chunk) {}

    Level &getLevel() const { return *mLevel; }

    LevelChunk &getChunk() const { return *mChunk; }

private:
    Level *mLevel;
    LevelChunk *mChunk;
};
