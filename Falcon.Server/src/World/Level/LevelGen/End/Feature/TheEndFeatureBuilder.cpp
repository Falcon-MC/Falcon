#include "World/Level/LevelGen/End/Feature/TheEndFeatureBuilder.h"

#include "World/Level/LevelGen/End/Feature/EndPodiumFeature.h"
#include "World/Level/LevelGen/End/Feature/TheEndDecorationFeature.h"
#include "World/Level/LevelGen/End/TheEndNoiseHolder.h"
#include "World/Level/LevelGen/Feature/ChunkGenerateContext.h"
#include "World/Level/LevelGen/Feature/GeneratorRoot.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <memory>
#include <vector>

void TheEndFeatureBuilder::apply(Level &level, LevelChunk &chunk, const TheEndNoiseHolder &noiseHolder,
                                 std::vector<GeneratedBlockChange> *overflow) {
    const int64_t seed = level.getSeed();
    const TheEndSimplexNoise &islandNoise = noiseHolder.getDecorationIslandNoise();

    std::vector<IFeaturePtr> features;
    features.push_back(std::make_shared<TheEndDecorationFeature>(seed, islandNoise));
    features.push_back(std::make_shared<EndPodiumFeature>());

    GeneratorRoot root(level);
    root.setOverflowSink(chunk.getX(), chunk.getZ(), overflow);

    ChunkGenerateContext context(level, chunk);

    for (const IFeaturePtr &feature: features) {
        feature->setRoot(&root);
        feature->apply(context);
    }

    root.applySubChunkUpdate();
}
