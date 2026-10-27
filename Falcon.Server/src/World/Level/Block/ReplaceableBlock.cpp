#include "World/Level/Block/ReplaceableBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ReplaceableBlock, 330);

#include "World/Level/Block/BlockIdentifier.h"

#include <unordered_set>

namespace {
    const std::unordered_set<std::string> &replaceableIdentifiers() {
        static const std::unordered_set<std::string> identifiers = {
                "minecraft:bubble_column", "minecraft:bush", "minecraft:crimson_roots", "minecraft:warped_roots",
                "minecraft:nether_sprouts", "minecraft:large_fern",
                "minecraft:tall_grass", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
                "minecraft:leaf_litter", "minecraft:glow_lichen", "minecraft:sculk_vein", "minecraft:resin_clump",
                "minecraft:seagrass", "minecraft:vine"
        };
        return identifiers;
    }

    const std::unordered_set<std::string> &shearsOnlyIdentifiers() {
        static const std::unordered_set<std::string> identifiers = {
                "minecraft:vine", "minecraft:nether_sprouts"
        };
        return identifiers;
    }

    const std::unordered_set<std::string> &shearsOrSilkTouchIdentifiers() {
        static const std::unordered_set<std::string> identifiers = {
                "minecraft:bush", "minecraft:short_dry_grass", "minecraft:tall_dry_grass"
        };
        return identifiers;
    }

    bool contains(const std::unordered_set<std::string> &identifiers, const std::string &identifier) {
        return identifiers.find(identifier) != identifiers.end();
    }
}

bool ReplaceableBlock::matches(const std::string &identifier) {
    return replaceableIdentifiers().find(identifier) != replaceableIdentifiers().end()
           || BlockIdentifier::startsWith(identifier, "minecraft:light_block");
}

bool ReplaceableBlock::canBeReplaced(const BlockState &state) const {
    (void) state;

    return true;
}

bool ReplaceableBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                std::vector<BlockDrop> &drops) const {
    (void) fortuneLevel;

    if (contains(shearsOnlyIdentifiers(), state.mName)) {
        if (isShears(tool))
            drops.push_back({state.mName, 1});
        return true;
    }

    if (contains(shearsOrSilkTouchIdentifiers(), state.mName)) {
        if (isShears(tool) || hasSilkTouch(tool))
            drops.push_back({state.mName, 1});
        return true;
    }

    if (state.mName == "minecraft:seagrass") {
        // Either half of tall seagrass drops two.
        const bool tall = state.mStates.getString("sea_grass_type", "default") != "default";
        if (isShears(tool))
            drops.push_back({state.mName, tall ? 2 : 1});
        return true;
    }

    if (state.mName == "minecraft:glow_lichen") {
        if (isShears(tool))
            drops.push_back({state.mName, multifaceCount(state)});
        return true;
    }

    if (state.mName == "minecraft:sculk_vein") {
        if (hasSilkTouch(tool))
            drops.push_back({state.mName, multifaceCount(state)});
        return true;
    }

    if (state.mName == "minecraft:resin_clump") {
        drops.push_back({state.mName, multifaceCount(state)});
        return true;
    }

    return false;
}
