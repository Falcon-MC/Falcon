#include "Level/Generator/Overworld/Feature/OverworldFeatureBuilder.h"

#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"
#include "Level/Generator/Feature/ChunkGenerateContext.h"
#include "Level/Generator/Feature/GenerateFeatureRegistry.h"
#include "Level/Generator/Feature/GeneratorRoot.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <algorithm>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
    const int32_t MAX_TRACKED_BIOME_ID = 1024;

    std::set<int32_t> collectBiomesInChunk(const LevelChunk &chunk) {
        std::vector<bool> seen((size_t) MAX_TRACKED_BIOME_ID, false);
        std::set<int32_t> biomes;

        for (int32_t x = 0; x < 16; x++) {
            for (int32_t z = 0; z < 16; z++) {
                const int32_t top = chunk.getHeight(x, z);
                if (top < LevelChunk::MIN_Y)
                    continue;

                for (int32_t y = std::min(top, LevelChunk::MAX_Y); y > LevelChunk::MIN_Y; y--) {
                    const int32_t biomeId = (int32_t) chunk.getBiomeAt(x, y, z);
                    if (biomeId < 0 || biomeId >= MAX_TRACKED_BIOME_ID) {
                        biomes.insert(biomeId);
                        continue;
                    }

                    if (seen[(size_t) biomeId])
                        continue;

                    seen[(size_t) biomeId] = true;
                    biomes.insert(biomeId);
                }
            }
        }

        return biomes;
    }
}

void OverworldFeatureBuilder::apply(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) {
    if (!BiomeChunkGenDataRegistry::isLoaded())
        return;

    GenerateFeatureRegistry::initialize();

    const std::set<int32_t> biomes = collectBiomesInChunk(chunk);

    std::unordered_set<std::string> seenIdentifiers;
    std::vector<BiomeFeatureEntry> sortedFeatures;

    for (int32_t biomeId: biomes) {
        const std::vector<BiomeFeatureEntry> *features =
                BiomeChunkGenDataRegistry::getConsolidatedFeatures(biomeId);
        if (features == nullptr)
            continue;

        for (const BiomeFeatureEntry &feature: *features) {
            if (!seenIdentifiers.insert(feature.mIdentifier).second)
                continue;

            sortedFeatures.push_back(feature);
        }
    }

    std::stable_sort(sortedFeatures.begin(), sortedFeatures.end(),
                     [](const BiomeFeatureEntry &left, const BiomeFeatureEntry &right) {
                         return left.mEvalOrder < right.mEvalOrder;
                     });

    GeneratorRoot root(level);
    root.setOverflowSink(chunk.getX(), chunk.getZ(), overflow);

    ChunkGenerateContext context(level, chunk);

    for (const BiomeFeatureEntry &data: sortedFeatures) {
        IFeaturePtr feature = GenerateFeatureRegistry::get(data.mIdentifier);
        if (feature == nullptr)
            feature = GenerateFeatureRegistry::get(data.mFeature);

        if (feature == nullptr)
            continue;

        feature->setRoot(&root);
        feature->apply(context);
    }

    root.applySubChunkUpdate();
}
