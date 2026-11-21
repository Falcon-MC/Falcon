#include "World/Actor/Movement/Systems/BlockClimberSystem.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <cmath>

namespace {
    struct LayerScan {
        bool mScaffolding = false;
        bool mAscendable = false;
    };

    // scaffolding resting on something counts as ascendable: not on air or water
    bool restsOnSomething(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *below = level.peekBlockPtr(x, y - 1, z);
        return below != nullptr && below->mName != BlockNames::AIR && !LiquidView(*below).isWater();
    }

    // the columns under the box at one layer, as the game scans them
    LayerScan scanLayer(Level &level, const ServerActor &actor, const AxisAlignedBB &box, int32_t y) {
        LayerScan scan;
        const bool standsOnPowderSnow = actor.canStandOnPowderSnow();
        for (int32_t x = (int32_t) std::floor(box.mMinX); x <= (int32_t) std::floor(box.mMaxX); ++x) {
            for (int32_t z = (int32_t) std::floor(box.mMinZ); z <= (int32_t) std::floor(box.mMaxZ); ++z) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
                if (block == nullptr)
                    continue;

                const BlockTraversal traversal = block->getTraversal();
                if (traversal == BlockTraversal::Scaffolding) {
                    scan.mScaffolding = true;
                    if (restsOnSomething(level, x, y, z))
                        scan.mAscendable = true;
                } else if (traversal == BlockTraversal::PowderSnow && standsOnPowderSnow) {
                    scan.mAscendable = true;
                }
            }
        }
        return scan;
    }
}

void BlockClimberSystem::tick(ServerNetworkHandler &owner, ServerActor &actor) {
    Level &level = owner.getLevelFor(actor);
    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(actor);
    const int32_t feetY = (int32_t) std::floor(box.mMinY);
    const LayerScan inside = scanLayer(level, actor, box, feetY);
    const LayerScan below = scanLayer(level, actor, box, feetY - 1);

    ActorFlags &flags = actor.getFlags();
    const bool changed = flags.get(ActorFlag::InScaffolding) != inside.mScaffolding
                         || flags.get(ActorFlag::OverScaffolding) != below.mScaffolding
                         || flags.get(ActorFlag::InAscendingBlock) != inside.mAscendable
                         || flags.get(ActorFlag::OverDescendingBlock) != below.mAscendable;
    if (!changed)
        return;

    flags.set(ActorFlag::InScaffolding, inside.mScaffolding);
    flags.set(ActorFlag::OverScaffolding, below.mScaffolding);
    flags.set(ActorFlag::InAscendingBlock, inside.mAscendable);
    flags.set(ActorFlag::OverDescendingBlock, below.mAscendable);
    owner.syncActorFlags(actor);
}
