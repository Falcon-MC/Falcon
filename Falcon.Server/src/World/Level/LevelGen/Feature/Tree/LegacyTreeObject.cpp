#include "World/Level/LevelGen/Feature/Tree/LegacyTreeObject.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyBirchTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyDarkOakTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyJungleTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyOakTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacySpruceTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyTallBirchTree.h"

#include <cmath>
#include <memory>

namespace {

    const char *OVERRIDABLE_BLOCKS[] = {
            "minecraft:air",
            "minecraft:acacia_leaves",
            "minecraft:azalea_leaves",
            "minecraft:birch_leaves",
            "minecraft:azalea_leaves_flowered",
            "minecraft:cherry_leaves",
            "minecraft:dark_oak_leaves",
            "minecraft:jungle_leaves",
            "minecraft:mangrove_leaves",
            "minecraft:oak_leaves",
            "minecraft:spruce_leaves",
            "minecraft:snow_layer",
            "minecraft:acacia_sapling",
            "minecraft:cherry_sapling",
            "minecraft:spruce_sapling",
            "minecraft:bamboo_sapling",
            "minecraft:oak_sapling",
            "minecraft:jungle_sapling",
            "minecraft:dark_oak_sapling",
            "minecraft:leaf_litter",
            "minecraft:wildflowers",
            "minecraft:pink_petals",
            "minecraft:tall_grass",
            "minecraft:birch_sapling",
            "minecraft:short_grass",
            "minecraft:dandelion",
            "minecraft:lily_of_the_valley",
            "minecraft:lilac",
            "minecraft:peony",
            "minecraft:rose_bush",
            "minecraft:large_fern",
            "minecraft:fern"
    };

}

void LegacyTreeObject::growTree(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                                TreeWoodType type, bool tall) {
    std::shared_ptr<LegacyTreeObject> tree;
    switch (type) {
        case TreeWoodType::SPRUCE:
            tree = std::make_shared<LegacySpruceTree>();
            break;
        case TreeWoodType::BIRCH:
            if (tall)
                tree = std::make_shared<LegacyTallBirchTree>();
            else
                tree = std::make_shared<LegacyBirchTree>();
            break;
        case TreeWoodType::DARK_OAK:
            tree = std::make_shared<LegacyDarkOakTree>(6.0f, 3);
            break;
        case TreeWoodType::JUNGLE:
            tree = std::make_shared<LegacyJungleTree>();
            break;
        default:
            tree = std::make_shared<LegacyOakTree>();
            break;
    }

    if (tree->canPlaceObject(manager, x, y, z, random))
        tree->placeObject(manager, x, y, z, random);
}

bool LegacyTreeObject::overridable(const BlockState &state) {
    for (const char *candidate: OVERRIDABLE_BLOCKS) {
        if (state.mName == candidate)
            return true;
    }

    return false;
}

TreeWoodType LegacyTreeObject::getType() const {
    return TreeWoodType::OAK;
}

bool LegacyTreeObject::canGenerateWithVines() const {
    switch (getType()) {
        case TreeWoodType::OAK:
        case TreeWoodType::SPRUCE:
        case TreeWoodType::JUNGLE:
        case TreeWoodType::DARK_OAK:
            return true;
        default:
            return false;
    }
}

void LegacyTreeObject::setRandomTreeWithVines(IRandom &random) {
    mTreeWithVines = canGenerateWithVines() && random.nextInt(TREE_WITH_VINES_CHANCE) == 0;
}

int32_t LegacyTreeObject::getTreeHeight() const {
    return mTreeHeight;
}

bool LegacyTreeObject::canPlaceObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    (void) random;

    int32_t radiusToCheck = 0;
    for (int32_t yy = 0; yy < getTreeHeight() + 3; ++yy) {
        if (yy == 1 || yy == getTreeHeight())
            ++radiusToCheck;

        for (int32_t xx = -radiusToCheck; xx < (radiusToCheck + 1); ++xx) {
            for (int32_t zz = -radiusToCheck; zz < (radiusToCheck + 1); ++zz) {
                if (!overridable(manager.getBlockAt(x + xx, y + yy, z + zz)))
                    return false;
            }
        }
    }

    return true;
}

bool LegacyTreeObject::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    placeObject(manager, x, y, z, random);
    return true;
}

void LegacyTreeObject::placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    setRandomTreeWithVines(random);
    placeTrunk(manager, x, y, z, random, getTreeHeight() - 1);

    for (int32_t yy = y - 3 + getTreeHeight(); yy <= y + getTreeHeight(); ++yy) {
        const double yOff = (double) (yy - (y + getTreeHeight()));
        const int32_t mid = (int32_t) (1 - yOff / 2);

        for (int32_t xx = x - mid; xx <= x + mid; ++xx) {
            const int32_t xOff = std::abs(xx - x);

            for (int32_t zz = z - mid; zz <= z + mid; ++zz) {
                const int32_t zOff = std::abs(zz - z);
                if (xOff == mid && zOff == mid && (yOff == 0 || random.nextInt(2) == 0))
                    continue;

                if (!isSolid(manager.getBlockAt(xx, yy, zz)))
                    manager.setBlockStateAt(xx, yy, zz, getLeafBlockState());
            }
        }
    }
}

void LegacyTreeObject::placeTrunk(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                                  int32_t trunkHeight) {
    (void) random;

    manager.setBlockStateAt(x, y - 1, z, VanillaBlocks::DIRT().toBlockState());

    for (int32_t yy = 0; yy < trunkHeight; ++yy) {
        if (overridable(manager.getBlockAt(x, y + yy, z))) {
            manager.setBlockStateAt(x, y + yy, z, getTrunkBlockState());
            if (mTreeWithVines)
                addVinesAroundLog(manager, x, y + yy, z);
        }
    }
}

BlockState LegacyTreeObject::getTrunkBlockState() const {
    switch (getType()) {
        case TreeWoodType::JUNGLE:
            return withPillarAxis(VanillaBlocks::JUNGLE_LOG().toBlockState(), "y");
        case TreeWoodType::DARK_OAK:
            return withPillarAxis(VanillaBlocks::DARK_OAK_LOG().toBlockState(), "y");
        case TreeWoodType::SPRUCE:
            return withPillarAxis(VanillaBlocks::SPRUCE_LOG().toBlockState(), "y");
        case TreeWoodType::ACACIA:
            return withPillarAxis(VanillaBlocks::ACACIA_LOG().toBlockState(), "y");
        case TreeWoodType::BIRCH:
            return withPillarAxis(VanillaBlocks::BIRCH_LOG().toBlockState(), "y");
        case TreeWoodType::CHERRY:
            return withPillarAxis(VanillaBlocks::CHERRY_LOG().toBlockState(), "y");
        case TreeWoodType::PALE_OAK:
            return withPillarAxis(VanillaBlocks::PALE_OAK_LOG().toBlockState(), "y");
        case TreeWoodType::MANGROVE:
            return withPillarAxis(VanillaBlocks::MANGROVE_LOG().toBlockState(), "y");
        default:
            return withPillarAxis(VanillaBlocks::OAK_LOG().toBlockState(), "y");
    }
}

BlockState LegacyTreeObject::getLeafBlockState() const {
    switch (getType()) {
        case TreeWoodType::BIRCH:
            return VanillaBlocks::BIRCH_LEAVES().toBlockState();
        case TreeWoodType::ACACIA:
            return VanillaBlocks::ACACIA_LEAVES().toBlockState();
        case TreeWoodType::JUNGLE:
            return VanillaBlocks::JUNGLE_LEAVES().toBlockState();
        case TreeWoodType::SPRUCE:
            return VanillaBlocks::SPRUCE_LEAVES().toBlockState();
        case TreeWoodType::DARK_OAK:
            return VanillaBlocks::DARK_OAK_LEAVES().toBlockState();
        case TreeWoodType::CHERRY:
            return VanillaBlocks::CHERRY_LEAVES().toBlockState();
        case TreeWoodType::PALE_OAK:
            return VanillaBlocks::PALE_OAK_LEAVES().toBlockState();
        default:
            return VanillaBlocks::OAK_LEAVES().toBlockState();
    }
}
