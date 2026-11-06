#include "World/Level/LevelGen/Nether/Feature/NetherFeatureBuilder.h"

#include "World/Level/LevelGen/Feature/ChunkGenerateContext.h"
#include "World/Level/LevelGen/Feature/GeneratorRoot.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Nether/Feature/BasaltDelta/BasaltPillarFeature.h"
#include "World/Level/LevelGen/Nether/Feature/BasaltDelta/DeltaFeature.h"
#include "World/Level/LevelGen/Nether/Feature/BasaltDelta/DeltaMagmaFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Crimson/CrimsonHugeFungusFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Crimson/CrimsonVegetationFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Crimson/WeepingVinesClusterFeature.h"
#include "World/Level/LevelGen/Nether/Feature/NetherDecoration.h"
#include "World/Level/LevelGen/Nether/Feature/Warped/TwistingVinesClusterFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Warped/WarpedHugeFungusFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Warped/WarpedVegetationFeature.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <memory>
#include <vector>

namespace {
    const std::vector<IFeaturePtr> &netherFeatures() {
        static thread_local const std::vector<IFeaturePtr> features = {
                std::make_shared<DeltaFeature>(),
                std::make_shared<BasaltPillarFeature>(),
                std::make_shared<DeltaMagmaFeature>(),
                std::make_shared<CrimsonHugeFungusFeature>(),
                std::make_shared<CrimsonVegetationFeature>(),
                std::make_shared<WeepingVinesClusterFeature>(),
                std::make_shared<WarpedHugeFungusFeature>(),
                std::make_shared<WarpedVegetationFeature>(),
                std::make_shared<TwistingVinesClusterFeature>()
        };

        return features;
    }
}

void NetherFeatureBuilder::apply(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) {
    GeneratorRoot root(level);
    root.setOverflowSink(chunk.getX(), chunk.getZ(), overflow);

    NetherDecoration decoration(root, chunk, (uint32_t) level.getSeed());
    decoration.placeBeforeBiomes();

    ChunkGenerateContext context(level, chunk);

    for (const IFeaturePtr &feature: netherFeatures()) {
        feature->setRoot(&root);
        feature->apply(context);
    }

    decoration.placeAfterBiomes();

    root.applySubChunkUpdate();
}
