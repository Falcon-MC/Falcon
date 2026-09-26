#include "Block/Blocks/BuddingAmethystBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockQuery.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Components/PlacementOrientation.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"

#include <string>

FALCON_REGISTER_BLOCK(BuddingAmethystBlock, 322);

using namespace BlockQuery;

namespace {
    const char *BLOCK_FACE = "minecraft:block_face";
    const int32_t BUD_GROWTH_CHANCE = 5;

    const char *BUD_TIERS[] = {
            "minecraft:small_amethyst_bud", "minecraft:medium_amethyst_bud", "minecraft:large_amethyst_bud",
            "minecraft:amethyst_cluster"
    };

    const Vector3i FACE_OFFSETS[] = {
            Vector3i(0, -1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1),
            Vector3i(0, 0, 1), Vector3i(-1, 0, 0), Vector3i(1, 0, 0)
    };

    BlockState budOfTier(int32_t tier, int32_t face) {
        BlockState bud;
        switch (tier) {
            case 0:
                bud = VanillaBlocks::SMALL_AMETHYST_BUD().toBlockState();
                break;
            case 1:
                bud = VanillaBlocks::MEDIUM_AMETHYST_BUD().toBlockState();
                break;
            case 2:
                bud = VanillaBlocks::LARGE_AMETHYST_BUD().toBlockState();
                break;
            default:
                bud = VanillaBlocks::AMETHYST_CLUSTER().toBlockState();
                break;
        }
        return DecorationSupport::withState(bud, BLOCK_FACE, PlacementOrientation::faceName(face));
    }

    int32_t budTier(const std::string &identifier) {
        for (int32_t tier = 0; tier < 4; ++tier) {
            if (identifier == BUD_TIERS[tier])
                return tier;
        }
        return -1;
    }
}

bool BuddingAmethystBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:budding_amethyst";
}

void BuddingAmethystBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                        const BlockState &state) const {
    (void) state;

    if (RandomTickSystem::nextInt(BUD_GROWTH_CHANCE) != 0)
        return;

    const int32_t face = RandomTickSystem::nextInt(6);
    const Vector3i &delta = FACE_OFFSETS[face];
    const Vector3i target(position.x + delta.x, position.y + delta.y, position.z + delta.z);
    if (!isInRange(level, target))
        return;

    const BlockState targetState = stateAt(level, target);

    if (DecorationSupport::isAir(targetState) || WaterBlock::isSource(targetState)) {
        if (!BlockChangeSystem::change(level, target, budOfTier(0, face), BlockChangeCause::Grow, true))
            return;
        if (WaterBlock::isSource(targetState)) {
            level.setBlockStateAtLayer(target.x, target.y, target.z, 1, WaterBlock::source());
            BlockActionHandler::broadcastBlockUpdate(owner, level, target, WaterBlock::source(), 1);
        }
        return;
    }

    const int32_t tier = budTier(targetState.mName);
    if (tier < 0 || tier >= 3)
        return;

    if (targetState.mStates.getString(BLOCK_FACE, "") != PlacementOrientation::faceName(face))
        return;

    BlockChangeSystem::change(level, target, budOfTier(tier + 1, face), BlockChangeCause::Grow, true);
}
