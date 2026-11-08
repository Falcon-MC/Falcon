#include "World/Actor/Movement/Systems/WaterDragSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

using namespace MovementPhysics;

void WaterDragSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    if (!context.waterTravel.active)
        return;
    state.velocity.y *= 0.8f;
    const MobEffectInstance *levitation = context.player.getEffects().get(MobEffectId::Levitation);
    // the game's LevitateSystem (client 26.60 sub_14336C200) tags the mob LevitateTravelFlagComponent;
    // PlayerWaterGravity (sub_1433661B0, query sub_143366240) excludes it
    if (levitation != nullptr) {
        state.velocity.y = state.velocity.y * 0.8f + 0.01f * (float) levitation->level();
        return;
    }
    if (!abilities.swimming)
        state.velocity.y -= 0.005f;
}
