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
#include "World/Level/LevelGen/Nether/Feature/GlowStoneFeature.h"
#include "World/Level/LevelGen/Nether/Feature/NetherFireFeature.h"
#include "World/Level/LevelGen/Nether/Feature/NetherSpringFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Ore/AncientDebrisLargeFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Ore/AncientDebrisSmallFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Ore/MagmaOreFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Ore/NetherBlackstoneFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Ore/NetherGoldOreFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Ore/NetherGravelFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Ore/NetherLavaOreFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Ore/NetherQuartzOreFeature.h"
#include "World/Level/LevelGen/Nether/Feature/Ore/SoulSandOreFeature.h"
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
                std::make_shared<GlowStoneFeature>(),
                std::make_shared<SoulSandOreFeature>(),
                std::make_shared<MagmaOreFeature>(),
                std::make_shared<NetherLavaOreFeature>(),
                std::make_shared<NetherFireFeature>(),
                std::make_shared<NetherSpringFeature>(),
                std::make_shared<NetherGoldOreFeature>(),
                std::make_shared<AncientDebrisSmallFeature>(),
                std::make_shared<AncientDebrisLargeFeature>(),
                std::make_shared<NetherQuartzOreFeature>(),
                std::make_shared<DeltaFeature>(),
                std::make_shared<BasaltPillarFeature>(),
                std::make_shared<DeltaMagmaFeature>(),
                std::make_shared<CrimsonHugeFungusFeature>(),
                std::make_shared<CrimsonVegetationFeature>(),
                std::make_shared<WeepingVinesClusterFeature>(),
                std::make_shared<WarpedHugeFungusFeature>(),
                std::make_shared<WarpedVegetationFeature>(),
                std::make_shared<TwistingVinesClusterFeature>(),
                std::make_shared<NetherBlackstoneFeature>(),
                std::make_shared<NetherGravelFeature>()
        };

        return features;
    }
}

void NetherFeatureBuilder::apply(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) {
    GeneratorRoot root(level);
    root.setOverflowSink(chunk.getX(), chunk.getZ(), overflow);

    ChunkGenerateContext context(level, chunk);

    for (const IFeaturePtr &feature: netherFeatures()) {
        feature->setRoot(&root);
        feature->apply(context);
    }

    root.applySubChunkUpdate();
}
