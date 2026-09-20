#include "Block/Blocks/GrowthBlocks.h"

#include "Block/BlockIdentifier.h"
#include "Block/BlockLightProperties.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Feature/Tree/LegacyTreeObject.h"
#include "Level/Generator/Random/SimpleRandom.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Types/ItemStack.h"

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
    const int MINIMUM_LIGHT_LEVEL = 9;
    const int MINIMUM_SPREAD_LIGHT_LEVEL = 4;
    const int MAXIMUM_SPREAD_LIGHT_FILTER = 2;
    const int LEAVES_SEARCH_DISTANCE = 7;

    int64_t randomSeed() {
        return (int64_t) std::chrono::steady_clock::now().time_since_epoch().count();
    }

    BlockState withState(const BlockState &state, const std::string &name, int32_t value) {
        Tag states = state.mStates;
        states.putInt(name, value);
        return BlockState(state.mName, states);
    }

    int lightFilterAt(Level &level, const Vector3i &position) {
        const BlockState above = level.getBlockState(position.x, position.y, position.z);
        return BlockLightProperties::lightFilter(BlockLightProperties::packed(above));
    }

    bool isTransparentAt(Level &level, const Vector3i &position) {
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        return BlockLightProperties::isTransparent(BlockLightProperties::packed(state));
    }

    bool isLogState(const BlockState &state) {
        return BlockIdentifier::endsWith(state.mName, "_log")
               || BlockIdentifier::endsWith(state.mName, "_wood")
               || BlockIdentifier::endsWith(state.mName, "_stem")
               || BlockIdentifier::endsWith(state.mName, "_hyphae")
               || state.mName == "minecraft:mangrove_roots";
    }

    bool isLeavesState(const BlockState &state) {
        return BlockIdentifier::endsWith(state.mName, "_leaves");
    }

    bool findLog(Level &level, const Vector3i &position, int distance, std::unordered_set<int64_t> &visited) {
        static const int OFFSETS[6][3] = {
                {0, 1, 0}, {0, -1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}
        };

        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (isLogState(state))
            return true;

        if (distance == 0 || !isLeavesState(state))
            return false;

        const int64_t key = BlockManager::hashXYZ(position.x, position.y, position.z) | (int64_t) distance;
        if (!visited.insert(key).second)
            return false;

        for (const auto &offset: OFFSETS) {
            const Vector3i side(position.x + offset[0], position.y + offset[1], position.z + offset[2]);
            if (findLog(level, side, distance - 1, visited))
                return true;
        }

        return false;
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

bool CropBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier, {
            "minecraft:wheat", "minecraft:carrots", "minecraft:potatoes", "minecraft:beetroot",
            "minecraft:torchflower_crop", "minecraft:pitcher_crop"
    });
}

bool NetherWartBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:nether_wart";
}

bool StemBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:pumpkin_stem" || identifier == "minecraft:melon_stem";
}

bool SaplingBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_sapling") && identifier != "minecraft:bamboo_sapling";
}

bool LeavesBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_leaves");
}

bool SpreadingBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:grass_block" || identifier == "minecraft:mycelium";
}

bool NyliumBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:crimson_nylium" || identifier == "minecraft:warped_nylium";
}

void CropBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) const {
    if (RandomTickSystem::nextInt(getGrowthChance()) != 0)
        return;

    if (needsLight() && RandomTickSystem::getFullLight(level, position) < MINIMUM_LIGHT_LEVEL)
        return;

    const int32_t growth = state.mStates.getInt(getGrowthState());
    if (growth >= getMaxGrowth())
        return;

    RedstoneSystem::setBlockState(owner, level, position, withState(state, getGrowthState(), growth + 1));
}

std::string StemBlock::getFruitIdentifier() const {
    return getIdentifier() == "minecraft:melon_stem" ? "minecraft:melon_block" : "minecraft:pumpkin";
}

void StemBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) const {
    if (RandomTickSystem::nextInt(2) != 0)
        return;

    if (RandomTickSystem::getFullLight(level, position) < MINIMUM_LIGHT_LEVEL)
        return;

    const int32_t growth = state.mStates.getInt("growth");
    if (growth < 7) {
        RedstoneSystem::setBlockState(owner, level, position, withState(state, "growth", growth + 1));
        return;
    }

    static const int SIDES[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    const std::string fruit = getFruitIdentifier();

    for (const auto &side: SIDES) {
        const BlockState neighbour = level.getBlockState(position.x + side[0], position.y, position.z + side[1]);
        if (neighbour.mName == fruit)
            return;
    }

    const int chosen = RandomTickSystem::nextInt(4);
    const Vector3i target(position.x + SIDES[chosen][0], position.y, position.z + SIDES[chosen][1]);

    if (level.getBlockState(target.x, target.y, target.z).mName != "minecraft:air")
        return;

    const BlockState below = level.getBlockState(target.x, target.y - 1, target.z);
    if (below.mName != "minecraft:farmland" && below.mName != "minecraft:grass_block"
        && below.mName != "minecraft:dirt")
        return;

    RedstoneSystem::setBlockState(owner, level, target, BlockState(fruit));
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
        RedstoneSystem::setBlockState(owner, level, position, BlockState(state.mName, states));
        return;
    }

    growTree(owner, level, position);
}

void LeavesBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    if (state.mStates.getByte("update_bit") == 0 || state.mStates.getByte("persistent_bit") != 0)
        return;

    std::unordered_set<int64_t> visited;
    if (findLog(level, position, LEAVES_SEARCH_DISTANCE, visited)) {
        Tag states = state.mStates;
        states.putByte("update_bit", 0);
        RedstoneSystem::setBlockState(owner, level, position, BlockState(state.mName, states));
        return;
    }

    BlockActionHandler::destroyBlock(owner, level, position, state, true, ItemStack::air());
}

void SpreadingBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    const Vector3i above(position.x, position.y + 1, position.z);

    if (lightFilterAt(level, above) > 1) {
        RedstoneSystem::setBlockState(owner, level, position, BlockState("minecraft:dirt"));
        return;
    }

    if (RandomTickSystem::getFullLight(level, above) < MINIMUM_LIGHT_LEVEL)
        return;

    const Vector3i target(position.x - 1 + RandomTickSystem::nextInt(3),
                          position.y - 3 + RandomTickSystem::nextInt(5),
                          position.z - 1 + RandomTickSystem::nextInt(3));

    if (level.getBlockState(target.x, target.y, target.z).mName != "minecraft:dirt")
        return;

    const Vector3i targetAbove(target.x, target.y + 1, target.z);
    if (RandomTickSystem::getFullLight(level, targetAbove) < MINIMUM_SPREAD_LIGHT_LEVEL
        || lightFilterAt(level, targetAbove) >= MAXIMUM_SPREAD_LIGHT_FILTER)
        return;

    RedstoneSystem::setBlockState(owner, level, target, BlockState(state.mName));
}

void NyliumBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    (void) state;

    if (isTransparentAt(level, Vector3i(position.x, position.y + 1, position.z)))
        return;

    RedstoneSystem::setBlockState(owner, level, position, BlockState("minecraft:netherrack"));
}
