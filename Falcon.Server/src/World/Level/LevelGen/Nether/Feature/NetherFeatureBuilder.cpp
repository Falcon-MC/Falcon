#include "World/Level/LevelGen/Nether/Feature/NetherFeatureBuilder.h"

#include "World/Level/LevelGen/Feature/GeneratorRoot.h"
#include "World/Level/LevelGen/Nether/Feature/NetherDecoration.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <vector>

void NetherFeatureBuilder::apply(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) {
    GeneratorRoot root(level);
    root.setOverflowSink(chunk.getX(), chunk.getZ(), overflow);

    const uint32_t seed = static_cast<uint32_t>(level.getSeed());
    NetherDecoration decoration(root, chunk, seed);
    decoration.placeBeforeBiomes();

    // sub_14CDABCD0 decorates with one biome, the one of the chunk's far corner, and gets the decoration random:
    // the ancient debris after it start where the rules' math.random left it
    OverworldRulePlacer::applyBiome(root, chunk, seed, static_cast<int32_t>(chunk.getBiomeAt(15, 0, 15)),
                                    decoration.getRandom());

    decoration.placeAfterBiomes();

    root.applySubChunkUpdate();
}
