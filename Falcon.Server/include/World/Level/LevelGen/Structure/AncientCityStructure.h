#pragma once

#include <cstdint>

class ChunkGenerator;
class Level;
class LevelChunk;
class TerrainBeard;

/**
 * Ancient cities. The game builds them in code: a city centre from its pool, grown seven pieces deep by the jigsaw
 * assembler of the bastions, its city anchor at y -27. The terrain is emptied inside the pieces and filled under them.
 */
namespace AncientCityStructure {
    void addBeard(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ, TerrainBeard &beard);

    void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
}
