#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class BlockState;
class Level;
struct Vector3i;

namespace json {
    struct Value;
}

// minecraft:placement_filter of a custom block: it stands only against the blocks a condition names, on the
// faces the condition allows
struct BlockPlacementFilter {
    struct Match {
        std::string mName;
        std::vector<std::pair<std::string, std::string>> mStates;
        // q.any_tag or q.all_tags of a tags filter
        std::vector<std::string> mTags;
        bool mAllTags = false;
    };

    struct Condition {
        // one bit per face, PlacementOrientation order
        uint8_t mFaces = 0x3F;
        // without a block filter any block that is not air; a filter whose entries all failed to parse holds nothing
        bool mAnyBlock = true;
        std::vector<Match> mBlocks;
    };

    std::vector<Condition> mConditions;

    static BlockPlacementFilter parse(const json::Value &component, const std::string &identifier);

    bool isEmpty() const { return mConditions.empty(); }

    // placed on a face of the clicked block
    bool allowsPlacement(Level &level, const Vector3i &position, int blockFace) const;

    // a neighbour changed: any allowed face still against a matching block; an unloaded neighbour is unknown
    bool survives(Level &level, const Vector3i &position) const;
};
