#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <cstdint>

class SurfaceOverwriteBuilder {
public:
    void apply(LevelChunk &chunk) const;

private:
    static bool _isSolidState(const BlockState &state);
};
