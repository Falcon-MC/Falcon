#include "World/Level/Block/CropBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CropBlock, 270);

#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/GrowthHelpers.h"
#include "World/Level/Block/BlockPickItem.h"
#include "World/Level/Block/DoubleBlock.h"

#include <algorithm>
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Level.h"

#include <string>

using namespace GrowthHelpers;

bool CropBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier,
                                      {"minecraft:wheat", "minecraft:carrots", "minecraft:potatoes",
                                       "minecraft:beetroot", "minecraft:torchflower_crop", "minecraft:pitcher_crop"});
}

void CropBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) const {
    if (RandomTickSystem::nextInt(getGrowthChance()) != 0)
        return;

    if (needsLight() && RandomTickSystem::getFullLight(level, position) < MINIMUM_LIGHT_LEVEL)
        return;

    if (!DoubleBlock::isUpperHalf(state))
        growBy(level, position, state, 1, nullptr);
}

bool CropBlock::growBy(Level &level, const Vector3i &position, const BlockState &state, int amount,
                       ServerPlayer *player) const {
    const int current = state.mStates.getInt(getGrowthState());
    if (current >= getMaxGrowth())
        return false;
    const int next = std::min(getMaxGrowth(), current + amount);
    const BlockState grown = state.mName == "minecraft:torchflower_crop" && next == getMaxGrowth()
                                     ? BlockState("minecraft:torchflower")
                                     : withState(state, getGrowthState(), next);
    return BlockChangeSystem::change(level, position, grown, BlockChangeCause::Grow, false, player);
}

bool CropBlock::getDrops(const BlockState &state, const ItemStack &, int32_t fortune,
                         std::vector<BlockDrop> &drops) const {
    const int growth = state.mStates.getInt(getGrowthState());
    const std::string seed = BlockPickItem::identifierFor(state.mName);
    if (growth < getMaxGrowth() || state.mName == "minecraft:torchflower_crop") {
        drops.push_back({seed, 1});
        return true;
    }

    const int seeds = rollSeeds(growth, 3 + fortune);
    if (state.mName == "minecraft:wheat" || state.mName == "minecraft:beetroot") {
        const int produce = state.mName == "minecraft:wheat" ? 1 : 1 + RandomTickSystem::nextInt(2);
        drops.push_back({state.mName, produce});
        if (seeds > 0)
            drops.push_back({seed, seeds});
    } else {
        drops.push_back({seed, 1 + RandomTickSystem::nextInt(fortune + 1) + seeds});
        if (state.mName == "minecraft:potatoes" && RandomTickSystem::nextInt(50) == 0)
            drops.push_back({"minecraft:poisonous_potato", 1});
    }
    return true;
}

int CropBlock::rollSeeds(int growth, int rolls) {
    int count = 0;
    for (int roll = 0; roll < rolls; ++roll) {
        if (RandomTickSystem::nextInt(15) <= growth)
            ++count;
    }
    return count;
}
