#include "World/Level/Block/SaplingBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SaplingBlock, 280);

#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/GrowthHelpers.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"
#include "World/Level/LevelGen/Feature/Tree/BeeNestGenerator.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

#include <optional>
#include <random>
#include <string>
#include <vector>

using namespace GrowthHelpers;

namespace {
    struct SaplingFeatures {
        const char *mSapling;
        const char *mSingle;
        const char *mSquare;
        const char *mFlowerVariant = nullptr;
    };

    const SaplingFeatures *featuresFor(const std::string &name) {
        static constexpr SaplingFeatures FEATURES[] = {
                {"minecraft:oak_sapling", "minecraft:random_oak_tree_from_sapling_feature", nullptr,
                 "minecraft:random_oak_tree_with_beehive_from_sapling_feature"},
                {"minecraft:birch_sapling", "minecraft:birch_tree_feature", nullptr,
                 "minecraft:birch_tree_with_beehive_feature"},
                {"minecraft:spruce_sapling", "minecraft:select_standing_spruce_tree_feature",
                 "minecraft:mega_spruce_tree_feature"},
                {"minecraft:jungle_sapling", "minecraft:select_undecorated_jungle_tree_feature",
                 "minecraft:mega_jungle_tree_feature"},
                {"minecraft:acacia_sapling", "minecraft:savanna_tree_feature", nullptr},
                {"minecraft:cherry_sapling", "minecraft:cherry_tree_feature", nullptr,
                 "minecraft:cherry_tree_with_beehive_feature"},
                {"minecraft:poplar_sapling", "minecraft:select_poplar_tree_feature", nullptr},
                {"minecraft:dark_oak_sapling", nullptr, "minecraft:select_roofed_tree_feature"},
                {"minecraft:pale_oak_sapling", nullptr, "minecraft:pale_oak_tree_feature"}};
        for (const SaplingFeatures &entry: FEATURES) {
            if (name == entry.mSapling)
                return &entry;
        }
        return nullptr;
    }

    std::optional<Vector3i> squareOrigin(Level &level, const Vector3i &at, const SaplingFeatures &features) {
        if (features.mSquare == nullptr)
            return std::nullopt;
        const auto sameSapling = [&](int x, int z) {
            const BlockState *state = level.peekBlockPtr(x, at.y, z);
            return state != nullptr && state->mName == features.mSapling;
        };
        for (int x = at.x; x >= at.x - 1; --x) {
            for (int z = at.z; z >= at.z - 1; --z) {
                if (sameSapling(x, z) && sameSapling(x + 1, z) && sameSapling(x, z + 1) && sameSapling(x + 1, z + 1))
                    return Vector3i(x, at.y, z);
            }
        }
        return std::nullopt;
    }

    class TrackingBlockManager : public BlockManager {
    public:
        explicit TrackingBlockManager(Level &level) : BlockManager(level) {}

        void setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) override {
            if (!isCached(x, y, z))
                mChanged.emplace_back(x, y, z);
            BlockManager::setBlockStateAt(x, y, z, state);
        }

        void populateBeeNest(int32_t x, int32_t y, int32_t z, int32_t count) override {
            mNest = Vector3i(x, y, z);
            mBeeCount = count;
        }

        void applyBeeNest() {
            if (mNest && getLevel().getBlockState(mNest->x, mNest->y, mNest->z).mName == "minecraft:bee_nest")
                BlockManager::populateBeeNest(mNest->x, mNest->y, mNest->z, mBeeCount);
        }

        const std::vector<Vector3i> &getChanged() const {
            return mChanged;
        }

    private:
        std::vector<Vector3i> mChanged;
        std::optional<Vector3i> mNest;
        int32_t mBeeCount = 0;
    };
}

bool SaplingBlock::matches(const std::string &identifier) {
    return featuresFor(identifier) != nullptr;
}

namespace {
    void growTree(Level &level, const Vector3i &position, ServerPlayer *player, const SaplingFeatures &features,
                  const std::optional<Vector3i> &square) {
        const Vector3i origin = square.value_or(position);
        const int width = square ? 2 : 1;
        const char *feature = square ? features.mSquare : features.mSingle;
        if (feature == nullptr)
            return;

        TrackingBlockManager manager(level);
        for (int dx = 0; dx < width; ++dx) {
            for (int dz = 0; dz < width; ++dz)
                manager.setBlockStateAt(origin.x + dx, origin.y, origin.z + dz, BlockState("minecraft:air"));
        }

        std::mt19937 random(static_cast<uint32_t>(RandomTickSystem::nextInt(INT32_MAX)));
        if (!square && features.mFlowerVariant && RandomTickSystem::nextFloat() < 0.05f
            && BeeNestGenerator::hasNearbyFlower(manager, origin.x, origin.y, origin.z))
            feature = features.mFlowerVariant;
        OverworldRulePlacer::placeFeature(manager, feature, origin.x, origin.y, origin.z, random);

        for (int dx = 0; dx < width; ++dx) {
            for (int dz = 0; dz < width; ++dz) {
                if (!BlockIdentifier::endsWith(manager.getBlockAt(origin.x + dx, origin.y, origin.z + dz).mName,
                                               "_log"))
                    return;
            }
        }
        for (const Vector3i &at: manager.getChanged()) {
            if (at.y < level.getMinY() || at.y > level.getMaxY() || !level.isChunkResident(at.x >> 4, at.z >> 4))
                return;
        }

        const BlockState grown = manager.getBlockAt(position.x, position.y, position.z);
        if (!BlockChangeSystem::allows(level, position, grown, BlockChangeCause::Grow, nullptr, player))
            return;

        BlockChangeSystem::StructureBlocks blocks;
        blocks.reserve(manager.getChanged().size());
        for (const Vector3i &at: manager.getChanged())
            blocks.emplace_back(at, manager.getBlockAt(at.x, at.y, at.z));
        if (BlockChangeSystem::prepareStructure(level, position, blocks, player)) {
            BlockChangeSystem::applyStructure(level, blocks, false);
            manager.applyBeeNest();
        }
    }
}

void SaplingBlock::onRandomTick(ServerNetworkHandler &, Level &level, const Vector3i &position,
                                const BlockState &state) const {
    if (RandomTickSystem::getFullLight(level, Vector3i(position.x, position.y + 1, position.z)) >= MINIMUM_LIGHT_LEVEL
        && RandomTickSystem::nextInt(7) == 0)
        advanceGrowth(level, position, state, nullptr, false);
}

bool SaplingBlock::fertilize(Level &level, const Vector3i &position, const BlockState &state,
                             ServerPlayer *player) const {
    const auto *features = featuresFor(state.mName);
    if (features == nullptr || (!features->mSingle && !squareOrigin(level, position, *features)))
        return false;
    const bool creative = player != nullptr && player->isCreative();
    if (creative || RandomTickSystem::nextFloat() < 0.45f)
        advanceGrowth(level, position, state, player, creative);
    return true;
}

void SaplingBlock::advanceGrowth(Level &level, const Vector3i &position, const BlockState &state, ServerPlayer *player,
                                 bool forceTree) const {
    if (!forceTree && state.mStates.getByte("age_bit") == 0) {
        Tag states = state.mStates;
        states.putByte("age_bit", 1);
        BlockChangeSystem::change(level, position, BlockState(state.mName, states), BlockChangeCause::Grow, false,
                                  player);
        return;
    }
    const auto *features = featuresFor(state.mName);
    if (features)
        growTree(level, position, player, *features, squareOrigin(level, position, *features));
}
