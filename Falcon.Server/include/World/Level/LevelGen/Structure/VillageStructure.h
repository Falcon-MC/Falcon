#pragma once

#include "World/Level/LevelGen/Structure/JigsawAssembler.h"

#include <cstdint>
#include <vector>

class ChunkGenerator;

/**
 * Villages. The start chunk draws a town centre for the style of its biome and the jigsaw assembler grows it from
 * there, at most six pieces deep.
 */
class VillageStructure {
public:
    static std::vector<JigsawPiece> plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ);
};
