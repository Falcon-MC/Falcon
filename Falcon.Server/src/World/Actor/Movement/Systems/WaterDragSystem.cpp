#include "World/Actor/Movement/Systems/WaterDragSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

using namespace MovementPhysics;

void WaterDragSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    if (!contact.waterTravel)
        return;
    state.velocity.y *= 0.8f;
    const MobEffectInstance *levitation = context.player.getEffects().get(MobEffectId::Levitation);
    if (levitation != nullptr)
        state.velocity.y = state.velocity.y * 0.8f + 0.01f * (float) levitation->level();
    if (!abilities.swimming)
        state.velocity.y -= 0.005f;
}
