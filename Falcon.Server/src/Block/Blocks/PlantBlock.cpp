#include "Block/Blocks/PlantBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(PlantBlock, 320);

#include "Block/BlockIdentifier.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Generator/Feature/IFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

#include <cstdlib>
#include <unordered_map>

namespace {
    const int32_t SHORT_GRASS_SEED_CHANCE = 8;

    struct PlantRule {
        PlantSupport mSupport;
        bool mReplaceable;
    };

    const std::unordered_map<std::string, PlantRule> &plantRules() {
        static const std::unordered_map<std::string, PlantRule> rules = {
                {"minecraft:allium", {PlantSupport::Dirt, false}},
                {"minecraft:azure_bluet", {PlantSupport::Dirt, false}},
                {"minecraft:blue_orchid", {PlantSupport::Dirt, false}},
                {"minecraft:closed_eyeblossom", {PlantSupport::Dirt, false}},
                {"minecraft:cornflower", {PlantSupport::Dirt, false}},
                {"minecraft:dandelion", {PlantSupport::Dirt, false}},
                {"minecraft:golden_dandelion", {PlantSupport::Dirt, false}},
                {"minecraft:lily_of_the_valley", {PlantSupport::Dirt, false}},
                {"minecraft:open_eyeblossom", {PlantSupport::Dirt, false}},
                {"minecraft:orange_tulip", {PlantSupport::Dirt, false}},
                {"minecraft:oxeye_daisy", {PlantSupport::Dirt, false}},
                {"minecraft:pink_petals", {PlantSupport::Dirt, false}},
                {"minecraft:pink_tulip", {PlantSupport::Dirt, false}},
                {"minecraft:poppy", {PlantSupport::Dirt, false}},
                {"minecraft:red_tulip", {PlantSupport::Dirt, false}},
                {"minecraft:torchflower", {PlantSupport::Dirt, false}},
                {"minecraft:white_tulip", {PlantSupport::Dirt, false}},
                {"minecraft:wildflowers", {PlantSupport::Dirt, false}},
                {"minecraft:wither_rose", {PlantSupport::DirtNetherrackSoulSand, false}},
                {"minecraft:acacia_sapling", {PlantSupport::Dirt, false}},
                {"minecraft:birch_sapling", {PlantSupport::Dirt, false}},
                {"minecraft:dark_oak_sapling", {PlantSupport::Dirt, false}},
                {"minecraft:jungle_sapling", {PlantSupport::Dirt, false}},
                {"minecraft:oak_sapling", {PlantSupport::Dirt, false}},
                {"minecraft:pale_oak_sapling", {PlantSupport::Dirt, false}},
                {"minecraft:spruce_sapling", {PlantSupport::Dirt, false}},
                {"minecraft:cherry_sapling", {PlantSupport::DirtWithoutLiquid, false}},
                {"minecraft:bamboo_sapling", {PlantSupport::DirtWithoutLiquid, false}},
                {"minecraft:short_grass", {PlantSupport::Dirt, true}},
                {"minecraft:fern", {PlantSupport::Dirt, true}},
                {"minecraft:deadbush", {PlantSupport::DirtSandClay, true}},
                {"minecraft:red_shrub", {PlantSupport::DirtSandClay, true}},
                {"minecraft:wheat", {PlantSupport::Farmland, false}},
                {"minecraft:carrots", {PlantSupport::Farmland, false}},
                {"minecraft:potatoes", {PlantSupport::Farmland, false}},
                {"minecraft:beetroot", {PlantSupport::Farmland, false}},
                {"minecraft:melon_stem", {PlantSupport::Farmland, false}},
                {"minecraft:pumpkin_stem", {PlantSupport::Farmland, false}},
                {"minecraft:torchflower_crop", {PlantSupport::Farmland, false}},
                {"minecraft:pitcher_crop", {PlantSupport::Farmland, false}},
                {"minecraft:nether_wart", {PlantSupport::SoulSand, false}},
                {"minecraft:brown_mushroom", {PlantSupport::Mushroom, false}},
                {"minecraft:red_mushroom", {PlantSupport::Mushroom, false}},
                {"minecraft:reeds", {PlantSupport::Reeds, false}},
                {"minecraft:cactus", {PlantSupport::Cactus, false}}
        };
        return rules;
    }

    bool isHardenedClay(const BlockState &state) {
        return state.mName == "minecraft:hardened_clay"
               || (BlockIdentifier::endsWith(state.mName, "_terracotta")
                   && !BlockIdentifier::endsWith(state.mName, "_glazed_terracotta"));
    }

    bool isNylium(const BlockState &state) {
        return state.mName == "minecraft:crimson_nylium" || state.mName == "minecraft:warped_nylium";
    }

    bool isCactusSideFree(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState side = level.getBlockState(x, y, z);
        return DecorationSupport::isAir(side) || !DecorationSupport::isSolid(side);
    }
}

PlantBlock::PlantBlock(const Block &block) : Block(block) {
}

bool PlantBlock::matches(const std::string &identifier) {
    return plantRules().find(identifier) != plantRules().end();
}

bool PlantBlock::canBeReplaced(const BlockState &state) const {
    const auto rule = plantRules().find(state.mName);
    return rule != plantRules().end() && rule->second.mReplaceable;
}

bool PlantBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    const auto rule = plantRules().find(getIdentifier());
    if (rule == plantRules().end())
        return true;

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);

    switch (rule->second.mSupport) {
        case PlantSupport::Dirt:
            return IFeature::isSupportDirt(below);
        case PlantSupport::DirtWithoutLiquid: {
            const BlockState current = level.getBlockState(position.x, position.y, position.z);
            const BlockState overlay = level.getBlockStateAtLayer(position.x, position.y, position.z, 1);
            return IFeature::isSupportDirt(below) && !LiquidView(current).isLiquid()
                   && !LiquidView(overlay).isLiquid();
        }
        case PlantSupport::DirtNetherrackSoulSand:
            return IFeature::isSupportDirt(below) || below.mName == "minecraft:netherrack"
                   || below.mName == "minecraft:soul_sand";
        case PlantSupport::DirtSandClay:
            return IFeature::isSupportDirt(below) || DecorationSupport::isSand(below) || isHardenedClay(below);
        case PlantSupport::Farmland:
            return below.mName == "minecraft:farmland";
        case PlantSupport::SoulSand:
            return below.mName == "minecraft:soul_sand";
        case PlantSupport::Mushroom:
            return below.mName == "minecraft:mycelium" || below.mName == "minecraft:podzol" || isNylium(below)
                   || !DecorationSupport::isTransparent(below);
        case PlantSupport::Reeds:
            return DecorationSupport::reedsSupportValid(level, below, position.x, position.y - 1, position.z);
        case PlantSupport::Cactus:
            return (DecorationSupport::isSand(below) || below.mName == "minecraft:cactus")
                   && isCactusSideFree(level, position.x, position.y, position.z - 1)
                   && isCactusSideFree(level, position.x, position.y, position.z + 1)
                   && isCactusSideFree(level, position.x - 1, position.y, position.z)
                   && isCactusSideFree(level, position.x + 1, position.y, position.z);
    }

    return true;
}

bool PlantBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
}

bool PlantBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                          std::vector<BlockDrop> &drops) const {
    if (state.mName == "minecraft:short_grass" || state.mName == "minecraft:fern") {
        drops = grassDrops(state, tool, fortuneLevel, SHORT_GRASS_SEED_CHANCE);
        return true;
    }

    if (state.mName == "minecraft:deadbush") {
        if (isShears(tool))
            drops.push_back({state.mName, 1});
        else
            drops.push_back({"minecraft:stick", rand() % 3});
        return true;
    }

    if (state.mName == "minecraft:red_shrub") {
        if (isShears(tool))
            drops.push_back({state.mName, 1});
        return true;
    }

    return false;
}
