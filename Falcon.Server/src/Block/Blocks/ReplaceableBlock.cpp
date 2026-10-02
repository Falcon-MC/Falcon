#include "Block/Blocks/ReplaceableBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ReplaceableBlock, 330);

#include "Block/BlockIdentifier.h"

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
                "minecraft:bush", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
                "minecraft:seagrass", "minecraft:vine", "minecraft:nether_sprouts"
        };
        return identifiers;
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

    if (shearsOnlyIdentifiers().find(state.mName) == shearsOnlyIdentifiers().end())
        return false;

    if (isShears(tool))
        drops.push_back({state.mName, 1});
    return true;
}
