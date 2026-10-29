#pragma once

#include "World/Level/Chunk/GeneratedBlockChange.h"

#include <vector>

class Level;
class LevelChunk;

class OverworldFeatureBuilder {
public:
    static void apply(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow = nullptr);
};
