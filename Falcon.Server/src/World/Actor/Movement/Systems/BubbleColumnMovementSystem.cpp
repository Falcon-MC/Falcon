#include "World/Actor/Movement/Systems/BubbleColumnMovementSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Level/Block/BlockQuery.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>

using namespace MovementPhysics;

void BubbleColumnMovementSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;

    if (context.abilities.flying)
        return;

    // the client pushes once per bubble column cell in the body, harder at the column's top
    const AxisAlignedBB box = boxAt(state.start, context.pose.mHeight);
    for (int32_t x = (int32_t) std::floor(box.mMinX); x < (int32_t) std::ceil(box.mMaxX); x++) {
        for (int32_t y = (int32_t) std::floor(box.mMinY); y < (int32_t) std::ceil(box.mMaxY); y++) {
            for (int32_t z = (int32_t) std::floor(box.mMinZ); z < (int32_t) std::ceil(box.mMaxZ); z++) {
                const LiquidInfo info = context.level.getLiquidInfo(x, y, z);
                if (!info.bubble)
                    continue;
                const bool surface = BlockQuery::isAirAt(context.level, Vector3i(x, y + 1, z));
                state.velocity.y = info.dragDown ? std::max(surface ? -0.9f : -0.3f, state.velocity.y - 0.03f)
                                                 : std::min(surface ? 1.8f : 0.7f,
                                                            state.velocity.y + (surface ? 0.1f : 0.06f));
            }
        }
    }
}
