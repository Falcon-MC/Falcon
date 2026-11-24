#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/BrushableBlockActor.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/Overworld/Terrain/TerrainBeard.h"
#include "World/Level/LevelGen/Structure/JigsawStructures.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void checkDefinitions() {
        const JigsawStructures &structures = JigsawStructures::get();
        require(structures.sets().size() == 3, "The game has three structure sets");

        const JigsawStructures::Structure *ruins = structures.structure("minecraft:trail_ruins");
        require(ruins != nullptr && ruins->mMaxDepth == 7 && ruins->mStartY == -15 && ruins->mAdapts
                        && ruins->mAdaptation == TerrainBeard::Adaptation::Bury
                        && ruins->mMaxDistanceHorizontal == 80
                        && ruins->mProjection == JigsawPlacer::Projection::WorldSurface,
                "The trail ruins are not read");
        const JigsawStructures::Structure *camp = structures.structure("minecraft:abandoned_camp_forest");
        require(camp != nullptr && camp->mMaxDepth == 3 && camp->mPadding == 10
                        && camp->mAdaptation == TerrainBeard::Adaptation::BeardThin && camp->mBiomeFilter.size() == 3,
                "A camp is not read");

        const JigsawPlacer::Pool *empty = structures.pool("minecraft:empty");
        require(empty != nullptr && empty->mSlots.empty(), "The empty pool is missing");
        const JigsawPlacer::Pool *towers = structures.pool("minecraft:trail_ruins/tower");
        require(towers != nullptr && towers->mSlots.size() == 5 && towers->mSlots[0] != nullptr
                        && towers->mSlots[0]->mProcessors == "minecraft:trail_ruins_houses_archaeology",
                "The tower pool is not read");
        const JigsawPlacer::Pool *trees = structures.pool("minecraft:abandoned_camp/trees/oak");
        require(trees != nullptr && trees->mSlots.size() == 1 && trees->mSlots[0]->mFeature == "minecraft:oak_tree_feature"
                        && trees->mSlots[0]->mLocation.empty(),
                "A feature element is not read");

        const JigsawStructures::ProcessorList *roads = structures.processors("minecraft:trail_ruins_roads_archaeology");
        require(roads != nullptr && roads->mRules.size() == 3 && roads->mCapped.size() == 1, "The roads lose a processor");
        require(roads->mRules[0].mInput == StructureProcessors::Test::RandomBlockMatch
                        && std::string(roads->mRules[0].mInputBlock) == "minecraft:gravel"
                        && std::string(roads->mRules[0].mOutput) == "minecraft:dirt"
                        && std::fabs(roads->mRules[0].mProbability - 0.2f) < 1e-6f,
                "The first road rule differs");
        const JigsawStructures::Capped &capped = roads->mCapped[0];
        require(capped.mLimit == 2 && capped.mReplacements.at("minecraft:gravel") == "minecraft:suspicious_gravel"
                        && capped.mLootTable == "loot_tables/entities/trail_ruins_brushable_block_common.json",
                "The capped road archaeology differs");

        const StructureFeature *feature = StructureFeature::get("abandoned_camp");
        require(feature != nullptr && feature->getSpacing() == 37, "The camps are not registered for /locate");

        const JigsawStructures::Structure *chambers = structures.structure("minecraft:trial_chambers");
        require(chambers != nullptr && chambers->mMaxDepth == 20 && chambers->mStartY == -40
                        && chambers->mStartYMax == -20 && chambers->mPadding == 10 && chambers->mAliases.size() == 3
                        && chambers->mAdaptation == TerrainBeard::Adaptation::Encapsulate,
                "The trial chambers are not registered");
        const JigsawPlacer::Pool *hallway = structures.pool("minecraft:trial_chambers/hallway");
        require(hallway != nullptr && hallway->mSlots.size() == 1225
                        && hallway->mFallback == "minecraft:trial_chambers/hallway/fallback",
                "The hallway pool differs");
        const JigsawPlacer::Pool *skeleton = structures.pool("minecraft:trial_chambers/spawner/ranged/skeleton");
        require(skeleton != nullptr && skeleton->mSlots.size() == 1
                        && skeleton->mSlots[0]->mLocation == "minecraft:trial_chambers/spawner/ranged/skeleton",
                "An alias target has no pool");
        const JigsawStructures::ProcessorList *bulbs =
                structures.processors("minecraft:trial_chambers_copper_bulb_degradation");
        require(bulbs != nullptr && bulbs->mRules.size() == 3, "The copper bulbs do not age");
        const StructureFeature *trial = StructureFeature::get("trial_chambers");
        require(trial != nullptr && trial->getSpacing() == 34, "The trial chambers are not registered for /locate");
    }

    void checkBeard() {
        TerrainBeard beard;
        // a bury piece from 0 to 9, its ground at 10
        beard.addPiece(0, 0, {0, 10, 0, 9, 19, 9, 0, TerrainBeard::Adaptation::Bury});
        require(std::fabs(beard.compute(4, 10, 4) - 1.0) < 1e-9, "A buried piece is not full at its ground");
        require(std::fabs(beard.compute(12, 10, 4) - 0.5) < 1e-6, "Three blocks out does not bury by half");
        require(beard.compute(15, 10, 4) == 0.0, "Six blocks out still buries");
        require(std::fabs(beard.compute(4, 16, 4) - 0.5) < 1e-6, "Six blocks up does not count as three");

        TerrainBeard thin;
        thin.addPiece(0, 0, {0, 10, 0, 9, 19, 9, 1, TerrainBeard::Adaptation::BeardThin});
        TerrainBeard joined = thin;
        joined.addJunction(0, 0, 4, 30, 4);
        require(joined.compute(4, 30, 4) != thin.compute(4, 30, 4), "A junction adds nothing");
        require(joined.compute(4, 60, 4) == thin.compute(4, 60, 4), "A far junction adds density");
    }

    void checkPlans(const std::string &name) {
        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, 42);
        const JigsawStructures &structures = JigsawStructures::get();
        const StructureFeature *feature = StructureFeature::get(name);
        require(feature != nullptr, name + " is not registered");

        StructureChunkPos start;
        require(feature->findNearest(*generator, 0, 0, start), "No " + name + " near the origin");
        const JigsawStructures::Set *set = nullptr;
        for (const std::unique_ptr<JigsawStructures::Set> &candidate: structures.sets()) {
            if (candidate->mIdentifier == "minecraft:" + name)
                set = candidate.get();
        }
        require(set != nullptr, name + " has no set");

        const std::shared_ptr<const JigsawStructures::Plan> plan = structures.plan(*generator, *set, start.x, start.z);
        require(plan != nullptr && !plan->mPieces.empty(), name + " found by /locate has no pieces");
        require(plan == structures.plan(*generator, *set, start.x, start.z), name + " is planned twice");
        require(plan->mPieces.size() > 1, name + " grew no piece from its start");

        const JigsawStructures::Structure &structure = *plan->mStructure;
        const JigsawPlacer::Piece &first = plan->mPieces.front();
        const int32_t centreX = (first.mBox.mMinX + first.mBox.mMaxX + 1) / 2;
        const int32_t centreZ = (first.mBox.mMinZ + first.mBox.mMaxZ + 1) / 2;
        for (const JigsawPlacer::Piece &piece: plan->mPieces) {
            require(piece.mDepth <= structure.mMaxDepth, name + " grows past its depth");
            if (!piece.mElement->mFeature.empty())
                continue;
            require(piece.mBox.mMinX >= centreX - structure.mMaxDistanceHorizontal - 1
                            && piece.mBox.mMaxX <= centreX + structure.mMaxDistanceHorizontal + 1
                            && piece.mBox.mMinZ >= centreZ - structure.mMaxDistanceHorizontal - 1
                            && piece.mBox.mMaxZ <= centreZ + structure.mMaxDistanceHorizontal + 1,
                    name + " grows past its distance");
        }

        // the start chunk gets blocks of the start piece
        Level level(name + "-test", 2, 1);
        const int32_t pieceChunkX = first.mBox.mMinX >> 4;
        const int32_t pieceChunkZ = first.mBox.mMinZ >> 4;
        LevelChunk &chunk = level.insertChunk(LevelChunk(pieceChunkX, pieceChunkZ));
        structures.place(*generator, level, chunk, nullptr);
        int32_t written = 0;
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t z = 0; z < 16; ++z) {
                for (int32_t y = first.mBox.mMinY; y <= first.mBox.mMaxY; ++y)
                    written += chunk.getBlock(x, y, z).mName != "minecraft:air" ? 1 : 0;
            }
        }
        require(written > 0, name + " wrote nothing in its start chunk");
    }
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        checkDefinitions();
        checkBeard();
        checkPlans("trail_ruins");
        checkPlans("abandoned_camp");
        checkPlans("trial_chambers");
    } catch (const std::exception &error) {
        std::clog << "Jigsaw structure regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
