#include "World/Actor/Movement/Systems/PlayerLiquidContactSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

using namespace MovementPhysics;

void PlayerLiquidContactSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &contact = context.contact;
    const float scale = context.player.getScale();
    const float bodyHeight = context.pose.mSneaking ? PLAYER_SNEAKING_HEIGHT : PLAYER_HEIGHT;

    contact.liquid = LiquidBlocksFetch::at(context.level, state.start,
                                           (abilities.swimming || abilities.gliding || context.pose.mCrawling),
                                           bodyHeight, scale);
    context.liquidTravel.lava = !abilities.flying && contact.liquid.lava;
    context.waterTravel.active = !abilities.flying && !context.liquidTravel.lava && contact.liquid.water;
}
