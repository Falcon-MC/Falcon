#include "World/Level/Block/CropBlock.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/DoubleBlock.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Level.h"

#include <algorithm>

namespace {
    class PitcherCropBlock final : public CropBlock {
    public:
        using CropBlock::CropBlock;

        static bool matches(const std::string &name) {
            return name == "minecraft:pitcher_crop";
        }

        bool growBy(Level &level, const Vector3i &position, const BlockState &state, int amount,
                    ServerPlayer *player) const override {
            if (DoubleBlock::isUpperHalf(state)) {
                const Vector3i base(position.x, position.y - 1, position.z);
                const BlockState lower = level.getBlockState(base.x, base.y, base.z);
                return lower.mName == state.mName && !DoubleBlock::isUpperHalf(lower)
                       && growBy(level, base, lower, amount, player);
            }
            const int current = state.mStates.getInt("growth");
            if (current >= 4)
                return false;
            const int next = std::min(4, current + amount);
            if (next < 3)
                return CropBlock::growBy(level, position, state, amount, player);
            const Vector3i top(position.x, position.y + 1, position.z);
            const BlockState *space = level.peekBlockPtr(top.x, top.y, top.z);
            if (top.y > level.getMaxY() || space == nullptr
                || (space->mName != "minecraft:air"
                    && !(space->mName == state.mName && DoubleBlock::isUpperHalf(*space))))
                return false;
            Tag states = state.mStates;
            states.putInt("growth", next);
            states.putByte("upper_block_bit", 0);
            BlockState lower(state.mName, states);
            states.putByte("upper_block_bit", 1);
            BlockState upper(state.mName, states);
            if (!BlockChangeSystem::allows(level, position, lower, BlockChangeCause::Grow, nullptr, player)
                || !BlockChangeSystem::allows(level, top, upper, BlockChangeCause::Grow, nullptr, player))
                return false;
            level.setBlock(position, lower, false);
            level.setBlock(top, upper, false);
            return true;
        }

        bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override {
            if (DoubleBlock::isUpperHalf(state))
                return DoubleBlock::isComplete(level, position, state);
            return CropBlock::canSurvive(level, position, state);
        }

        std::vector<Vector3i> getAffectedBlocks(Level &level, const Vector3i &position,
                                                const BlockState &state) const override {
            return DoubleBlock::otherHalf(level, position, state);
        }

        bool getDrops(const BlockState &state, const ItemStack &, int32_t,
                      std::vector<BlockDrop> &drops) const override {
            drops.push_back(
                    {state.mStates.getInt("growth") >= 4 ? "minecraft:pitcher_plant" : "minecraft:pitcher_pod", 1});
            return true;
        }
    };
}

FALCON_REGISTER_BLOCK(PitcherCropBlock, 269);
