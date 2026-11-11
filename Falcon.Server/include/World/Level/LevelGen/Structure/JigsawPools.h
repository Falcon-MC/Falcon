#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct JigsawElementData {
    const char *mName;
    // null for an element that places a feature
    const char *mTemplate;
    const char *mFeature;
    // comma separated
    const char *mProcessors;
    bool mTerrainMatching;
};

struct JigsawPoolItemData {
    const char *mElement;
    int32_t mWeight;
};

struct JigsawPoolEntryData {
    const char *mName;
    const char *mFallback;
    std::vector<JigsawPoolItemData> mItems;
};

namespace JigsawPoolData {
    extern const std::vector<JigsawElementData> ELEMENTS;
    extern const std::vector<JigsawPoolEntryData> POOLS;
}

/**
 * The template pools of the jigsaw structures. An element is drawn by its slot, so a pool repeats an element as many
 * times as its weight, and a null slot is the empty element.
 */
class JigsawPools {
public:
    struct Pool {
        std::string mFallback;
        std::vector<const JigsawElementData *> mSlots;
    };

    // null when the game has no pool of that name, which is how minecraft:empty ends a branch
    static const Pool *get(const std::string &name);

private:
    static const std::unordered_map<std::string, Pool> &_pools();
};
