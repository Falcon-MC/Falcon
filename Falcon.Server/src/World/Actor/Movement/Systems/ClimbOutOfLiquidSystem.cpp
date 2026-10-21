#include "World/Actor/Movement/Systems/ClimbOutOfLiquidSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Level/Level.h"

#include <cmath>

void ClimbOutOfLiquidSystem::tick(PlayerMovementContext &context) {
    if ((!context.contact.waterTravel && !context.contact.lavaTravel)
        || (!context.travel.moved.mCollidedX && !context.travel.moved.mCollidedZ))
        return;

    const auto &state = context.state;
    // BDS 0x14103CD10 probes from the moved box using post-drag velocity and the rise this input.
    const float lift = 0.6f - (state.simulated.y - state.start.y) + state.velocity.y;
    const AxisAlignedBB probe = context.travel.box.offset(state.velocity.x, lift, state.velocity.z);
    if (MovementPhysics::collides(context.level, probe))
        return;
    for (int32_t x = (int32_t) std::floor(probe.mMinX); x < (int32_t) std::ceil(probe.mMaxX); ++x) {
        for (int32_t y = (int32_t) std::floor(probe.mMinY); y < (int32_t) std::ceil(probe.mMaxY); ++y) {
            for (int32_t z = (int32_t) std::floor(probe.mMinZ); z < (int32_t) std::ceil(probe.mMaxZ); ++z) {
                if (context.level.peekBlockPtr(x, y, z) == nullptr)
                    return;
                const LiquidInfo liquid = context.level.getLiquidInfo(x, y, z);
                if (liquid.water || liquid.lava)
                    return;
            }
        }
    }
    context.state.velocity.y = 0.3f;
}
