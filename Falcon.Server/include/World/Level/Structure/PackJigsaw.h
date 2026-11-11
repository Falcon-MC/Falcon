#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Structure/McStructure.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

class Level;
class ServerNetworkHandler;

// the data-driven jigsaws of behavior packs: worldgen/template_pools, worldgen/jigsaw_structures and
// worldgen/processors, assembled from the packs' .mcstructure pieces
class PackJigsaw {
public:
    struct Rule {
        // minecraft:always_true, minecraft:block_match or minecraft:random_block_match
        std::string mPredicate = "minecraft:always_true";
        std::string mBlock;
        float mProbability = 1.0f;
        std::string mLocationBlock;
        BlockState mOutput;
    };

    // minecraft:rule, minecraft:block_ignore, minecraft:protected_blocks or minecraft:capped
    struct Processor {
        std::string mType;
        std::vector<Rule> mRules;
        // block_ignore: template blocks left out; protected_blocks: world blocks left in place
        std::vector<std::string> mBlocks;
        std::shared_ptr<Processor> mDelegate;
        int32_t mLimitMin = 0;
        int32_t mLimitMax = 0;
    };

    struct Element {
        // empty for minecraft:empty_pool_element
        std::string mLocation;
        std::string mProcessors;
        int32_t mWeight = 1;
    };

    struct Pool {
        std::vector<Element> mElements;
        std::string mFallback;
    };

    struct Structure {
        std::string mStartPool;
        int32_t mMaxDepth = 1;
        // nothing set: the placement height stays
        bool mHasStartHeight = false;
        // an int provider: constant gives min == max, uniform both bounds inclusive
        int32_t mStartHeightMin = 0;
        int32_t mStartHeightMax = 0;
    };

    struct Options {
        bool mIncludeEntities = true;
        bool mKeepJigsaws = false;
        bool mApplyWaterlogging = true;
    };

    struct Box {
        Vector3i mMin;
        Vector3i mMax;
    };

    // structure lookup: the registry of world.structureManager
    using Finder = std::function<std::shared_ptr<McStructure>(const std::string &)>;

    void addPack(const std::string &packRoot);

    const Pool *pool(const std::string &identifier) const;

    const Structure *structure(const std::string &identifier) const;

    const std::vector<Processor> *processors(const std::string &identifier) const;

    // places the pieces; false with the error text when nothing could start
    bool place(ServerNetworkHandler &owner, Level &level, const std::string &startPool, const std::string &startJigsaw,
               int32_t maxDepth, const Vector3i &location, const Options &options, const Finder &finder, int64_t seed,
               Box &bounds, std::string &error) const;

    void clear() {
        mPools.clear();
        mStructures.clear();
        mProcessors.clear();
    }

private:
    std::map<std::string, Pool> mPools;
    std::map<std::string, Structure> mStructures;
    std::map<std::string, std::vector<Processor>> mProcessors;
};
