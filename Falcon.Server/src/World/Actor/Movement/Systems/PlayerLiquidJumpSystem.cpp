#include "World/Actor/Movement/Systems/PlayerLiquidJumpSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

using namespace MovementPhysics;

void PlayerLiquidJumpSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    if (context.waterTravel.active || context.liquidTravel.lava) {
        if (input.jumpHeld) {
            const float amount = context.player.getSwimAmount().mCurrent;
            // BDS MobJumpSystem (0x140EB1110) also clamps jumps during both pose transitions.
            if ((abilities.swimming && !contact.liquid.eyeSubmerged)
                || (amount > 0.0f && amount < 1.0f)) {
                if (context.waterTravel.active)
                    state.velocity.y = 0.0f;
                return;
            }
            state.velocity.y += 0.04f;
        }
    }
}
