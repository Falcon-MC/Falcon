#pragma once

#include <cstdint>

class LevelChunk;

// the end of the game's chunk post-processing, once every feature pass has run: the water at the top of each column
// of the chunk freezes and snow falls on it where it is cold enough (sub_14CDD4880)
class OverworldFreezer {
public:
    static void apply(LevelChunk &chunk);

private:
    static int32_t _topRainBlock(const LevelChunk &chunk, int32_t x, int32_t z);

    static bool _isCold(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z);

    static bool _shouldFreeze(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z);

    static bool _shouldSnow(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z);
};
