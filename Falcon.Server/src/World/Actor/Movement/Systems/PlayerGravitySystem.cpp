#include "World/Actor/Movement/Systems/PlayerGravitySystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

using namespace MovementPhysics;

void PlayerGravitySystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &contact = context.contact;
    auto &travel = context.travel;

    if (abilities.flying || context.waterTravel.active || context.liquidTravel.lava
        || abilities.gliding || context.autoClimb.scaffoldDescend)
        return;
    const MobEffectInstance *levitation = context.player.getEffects().get(MobEffectId::Levitation);
    if (levitation != nullptr) {
        // BDS applies LevitateSystem (0x14104F680), then ground/air vertical drag (0x141051E00).
        state.velocity.y = (state.velocity.y * 0.8f + 0.01f * (float) levitation->level()) * GRAVITY_MULTIPLIER;
    } else {
        const float gravity = context.player.hasEffect(MobEffectId::SlowFalling) && state.velocity.y < 0.0f
                              ? SLOW_FALLING_GRAVITY : GRAVITY;
        state.velocity.y = (state.velocity.y - gravity) * GRAVITY_MULTIPLIER;
    }
}
