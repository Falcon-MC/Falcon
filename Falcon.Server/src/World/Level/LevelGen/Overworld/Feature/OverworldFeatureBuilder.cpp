#include "World/Level/LevelGen/Overworld/Feature/OverworldFeatureBuilder.h"

#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Feature/ChunkGenerateContext.h"
#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"
#include "World/Level/LevelGen/Feature/GeneratorRoot.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldFreezer.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

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

    // the chunk's biomes, bottom section first, each section's cells in the order the generator fills them; the
    // game lists them from its sections' palettes (sub_14386B520), whose order is not reproduced exactly
    std::vector<int32_t> biomesInSectionOrder(const LevelChunk &chunk) {
        std::vector<int32_t> biomes;
        for (int32_t sectionY = LevelChunk::MIN_Y; sectionY <= LevelChunk::MAX_Y; sectionY += 16) {
            for (int32_t x = 0; x < 16; x += 4) {
                for (int32_t z = 0; z < 16; z += 4) {
                    for (int32_t y = sectionY; y < sectionY + 16; y += 4) {
                        const int32_t biome = (int32_t) chunk.getBiomeAt(x, y, z);
                        if (std::find(biomes.begin(), biomes.end(), biome) == biomes.end())
                            biomes.push_back(biome);
                    }
                }
            }
        }
        return biomes;
    }

    // the passes the game places before underground_pass
    bool beforeUnderground(const BiomeFeatureEntry &feature) {
        return feature.mPass == "pregeneration_pass" || feature.mPass == "first_pass"
               || feature.mPass == "before_underground_pass";
    }
}

void OverworldFeatureBuilder::apply(Level &level, LevelChunk &chunk, const OverworldClimate &climate,
                                    std::vector<GeneratedBlockChange> *overflow) {
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
            if (OverworldRulePlacer::handles(feature))
                continue;

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

    const auto place = [&](bool early) {
        for (const BiomeFeatureEntry &data: sortedFeatures) {
            if (beforeUnderground(data) != early)
                continue;

            IFeaturePtr feature = GenerateFeatureRegistry::get(data.mIdentifier);
            if (feature == nullptr)
                feature = GenerateFeatureRegistry::get(data.mFeature);

            if (feature == nullptr)
                continue;

            feature->setRoot(&root);
            feature->apply(context);
        }
    };

    place(true);
    // the ores read the chunk itself, so the first features must be in it before they run
    root.applySubChunkUpdate();

    // applied at once so the features placed after them see the rules' blocks
    BlockManager rules(level);
    rules.setOverflowSink(chunk.getX(), chunk.getZ(), overflow);
    OverworldRulePlacer::apply(rules, chunk, climate, (uint32_t) level.getSeed(), biomesInSectionOrder(chunk));
    rules.applySubChunkUpdate();

    place(false);

    root.applySubChunkUpdate();
    OverworldFreezer::apply(chunk);
}
