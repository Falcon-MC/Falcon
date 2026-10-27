#include "World/Actor/Movement/Systems/PlayerPostTravelSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Components/BlockBehavior.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>

using namespace MovementPhysics;

namespace MovementPhysics {
    void slideAlongHoney(Level &level, const AxisAlignedBB &box, Vector3f &velocity) {
        const AxisAlignedBB contact = box.expand(HONEY_CONTACT_MARGIN, 0.0f, HONEY_CONTACT_MARGIN);
        for (int32_t x = (int32_t) std::floor(contact.mMinX); x < (int32_t) std::ceil(contact.mMaxX); x++) {
            for (int32_t y = (int32_t) std::floor(contact.mMinY); y < (int32_t) std::ceil(contact.mMaxY); y++) {
                for (int32_t z = (int32_t) std::floor(contact.mMinZ); z < (int32_t) std::ceil(contact.mMaxZ); z++) {
                    const Block *block = blockAt(level, (float) x, (float) y, (float) z);
                    if (block == nullptr || !block->getBehavior().slidesAlongSides())
                        continue;
                    velocity.x *= HONEY_HORIZONTAL_SLIDE;
                    velocity.y = std::max(HONEY_MAX_FALL, velocity.y);
                    velocity.z *= HONEY_HORIZONTAL_SLIDE;
                }
            }
        }
    }
}

void PlayerPostTravelSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &travel = context.travel;

    if (!abilities.spectator)
        slideAlongHoney(context.level, travel.box, state.velocity);

    context.player.setJumpDelay(state.jumpDelay);
}
