#pragma once

#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/LevelGen/Overworld/Terrain/TerrainBeard.h"
#include "World/Level/LevelGen/Structure/JigsawPlacer.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureProcessors.h"

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace json {
    struct Value;
}

class ChunkGenerator;
class Level;
class LevelChunk;

/**
 * The structures the game reads from its worldgen definitions (BedrockData worldgen.json): trail ruins and the
 * abandoned camps. A structure set draws one candidate chunk per region, picks one of its structures by weight and
 * grows it with the jigsaw placer; each chunk then writes the part of the pieces inside it.
 */
class JigsawStructures {
public:
    // a capped processor of the game: at most mLimit blocks of a piece, drawn at random, become another block
    struct Capped {
        // template block name to the block it becomes
        std::map<std::string, std::string> mReplacements;
        int32_t mLimit = 0;
        // given to the brushable block a replacement leaves, empty for none
        std::string mLootTable;
    };

    struct ProcessorList {
        // the rules point into it: a list keeps its strings in place
        std::list<std::string> mStrings;
        std::vector<StructureProcessors::Rule> mRules;
        std::vector<Capped> mCapped;
    };

    struct BiomeTest {
        std::string mTag;
        bool mEquals = true;
    };

    struct Structure {
        std::string mIdentifier;
        // every test must hold
        std::vector<BiomeTest> mBiomeFilter;
        std::string mStartPool;
        std::string mStartJigsaw;
        int32_t mMaxDepth = 0;
        int32_t mMaxDistanceHorizontal = 0;
        int32_t mMaxDistanceVertical = 0;
        int32_t mStartY = 0;
        int32_t mPadding = 0;
        JigsawPlacer::Projection mProjection = JigsawPlacer::Projection::None;
        TerrainBeard::Adaptation mAdaptation = TerrainBeard::Adaptation::BeardThin;
        bool mAdapts = false;
    };

    struct WeightedStructure {
        const Structure *mStructure = nullptr;
        int32_t mWeight = 1;
    };

    struct Set {
        std::string mIdentifier;
        int32_t mSalt = 0;
        int32_t mSpacing = 1;
        int32_t mSeparation = 0;
        StructureFeature::Spread mSpread = StructureFeature::Spread::Linear;
        float mFrequency = 1.0f;
        std::vector<WeightedStructure> mStructures;
    };

    // a planned start: the structure the set drew and its pieces
    struct Plan {
        const Structure *mStructure = nullptr;
        int32_t mStartChunkX = 0;
        int32_t mStartChunkZ = 0;
        std::vector<JigsawPlacer::Piece> mPieces;
    };

    // the definitions of the game; the document is BedrockData's worldgen.json
    static JigsawStructures &get();

    void load(const json::Value &document);

    const JigsawPlacer::Pool *pool(const std::string &name) const;

    const ProcessorList *processors(const std::string &name) const;

    const Structure *structure(const std::string &name) const;

    const std::vector<std::unique_ptr<Set>> &sets() const {
        return mSets;
    }

    // null when the chunk starts none of the set's structures
    std::shared_ptr<const Plan> plan(const ChunkGenerator &generator, const Set &set, int32_t chunkX,
                                     int32_t chunkZ) const;

    void addBeard(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ, TerrainBeard &beard) const;

    void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk,
               std::vector<GeneratedBlockChange> *overflow) const;

    // the biome tags the structure asks for, checked on one biome
    static bool passes(const Structure &structure, int32_t biomeId);

private:
    std::shared_ptr<const Plan> _generate(const ChunkGenerator &generator, const Structure &structure,
                                          int32_t chunkX, int32_t chunkZ) const;

    std::vector<std::shared_ptr<const Plan>> _plansNear(const ChunkGenerator &generator, int32_t chunkX,
                                                        int32_t chunkZ) const;

    std::map<std::string, JigsawPlacer::Pool> mPools;
    std::vector<std::unique_ptr<JigsawPlacer::Element>> mElements;
    std::map<std::string, ProcessorList> mProcessors;
    std::map<std::string, Structure> mStructures;
    std::vector<std::unique_ptr<Set>> mSets;
    std::vector<std::unique_ptr<StructureFeature>> mFeatures;
};
