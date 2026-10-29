#include "World/Level/LevelGen/End/Feature/TheEndFeatureBuilder.h"

#include "World/Level/LevelGen/End/Feature/ChorusPlantFeature.h"
#include "World/Level/LevelGen/End/Feature/EndGatewayFeature.h"
#include "World/Level/LevelGen/End/Feature/EndIslandFeature.h"
#include "World/Level/LevelGen/End/Feature/EndPodiumFeature.h"
#include "World/Level/LevelGen/End/Feature/SpikeFeature.h"
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
    features.push_back(std::make_shared<SpikeFeature>(seed));
    features.push_back(std::make_shared<EndPodiumFeature>());
    features.push_back(std::make_shared<ChorusPlantFeature>(seed, islandNoise));
    features.push_back(std::make_shared<EndGatewayFeature>(seed, islandNoise));
    features.push_back(std::make_shared<EndIslandFeature>(seed, islandNoise));

    GeneratorRoot root(level);
    root.setOverflowSink(chunk.getX(), chunk.getZ(), overflow);

    ChunkGenerateContext context(level, chunk);

    for (const IFeaturePtr &feature: features) {
        feature->setRoot(&root);
        feature->apply(context);
    }

    root.applySubChunkUpdate();
}
