#include "Block/Blocks/SaplingBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SaplingBlock, 280);

#include "Block/BlockIdentifier.h"
#include "Block/Blocks/GrowthHelpers.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Feature/Tree/LegacyTreeObject.h"
#include "Level/Generator/Random/SimpleRandom.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"

#include <chrono>
#include <string>
#include <vector>

using namespace GrowthHelpers;

namespace {
    int64_t randomSeed() {
        return (int64_t) std::chrono::steady_clock::now().time_since_epoch().count();
    }

    class TrackingBlockManager : public BlockManager {
    public:
        explicit TrackingBlockManager(Level &level) : BlockManager(level) {}

        void setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) override {
            BlockManager::setBlockStateAt(x, y, z, state);
            mChanged.push_back(Vector3i(x, y, z));
        }

        const std::vector<Vector3i> &getChanged() const { return mChanged; }

    private:
        std::vector<Vector3i> mChanged;
    };
}

bool SaplingBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_sapling") && identifier != "minecraft:bamboo_sapling";
}

TreeWoodType SaplingBlock::getWoodType() const {
    const std::string &identifier = getIdentifier();

    if (identifier == "minecraft:spruce_sapling")
        return TreeWoodType::SPRUCE;
    if (identifier == "minecraft:birch_sapling")
        return TreeWoodType::BIRCH;
    if (identifier == "minecraft:jungle_sapling")
        return TreeWoodType::JUNGLE;
    if (identifier == "minecraft:acacia_sapling")
        return TreeWoodType::ACACIA;
    if (identifier == "minecraft:dark_oak_sapling")
        return TreeWoodType::DARK_OAK;
    if (identifier == "minecraft:cherry_sapling")
        return TreeWoodType::CHERRY;
    if (identifier == "minecraft:pale_oak_sapling")
        return TreeWoodType::PALE_OAK;

    return TreeWoodType::OAK;
}

bool SaplingBlock::growTree(ServerNetworkHandler &owner, Level &level, const Vector3i &position) const {
    TrackingBlockManager manager(level);
    SimpleRandom random(randomSeed());

    LegacyTreeObject::growTree(manager, position.x, position.y, position.z, random, getWoodType(),
                               random.nextInt(10) == 0);

    if (manager.getChanged().empty())
        return false;

    const std::vector<Vector3i> changed = manager.getChanged();
    manager.applySubChunkUpdate();

    for (const Vector3i &changedPosition: changed) {
        const BlockState state = level.getBlockState(changedPosition.x, changedPosition.y, changedPosition.z);
        BlockActionHandler::broadcastBlockUpdate(owner, level, changedPosition, state);
    }

    return true;
}

void SaplingBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state) const {
    if (RandomTickSystem::nextInt(7) != 0)
        return;

    if (RandomTickSystem::getFullLight(level, Vector3i(position.x, position.y + 1, position.z))
        < MINIMUM_LIGHT_LEVEL)
        return;

    if (state.mStates.getByte("age_bit") == 0) {
        Tag states = state.mStates;
        states.putByte("age_bit", 1);
        level.setBlock(position, BlockState(state.mName, states), false);
        return;
    }

    growTree(owner, level, position);
}
