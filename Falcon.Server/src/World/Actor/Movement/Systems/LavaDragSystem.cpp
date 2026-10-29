#include "World/Actor/Movement/Systems/LavaDragSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

using namespace MovementPhysics;

void LavaDragSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &contact = context.contact;

    if (!context.liquidTravel.lava)
        return;
    state.velocity.y *= 0.5f;
    const MobEffectInstance *levitation = context.player.getEffects().get(MobEffectId::Levitation);
    if (levitation != nullptr)
        state.velocity.y = state.velocity.y * 0.8f + 0.01f * (float) levitation->level();
    state.velocity.y -= 0.02f;
}
