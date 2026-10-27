#include "World/Actor/Movement/Systems/PlayerLiquidContactSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void PlayerLiquidContactSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    contact.liquid = LiquidBlocksFetch::at(context.level, state.start, (abilities.swimming || abilities.gliding || context.pose.mCrawling),
                                            context.pose.mHeight);
    context.liquidTravel.lava = !abilities.flying && contact.liquid.lava;
    context.waterTravel.active = !abilities.flying && !context.liquidTravel.lava && contact.liquid.water;
}
