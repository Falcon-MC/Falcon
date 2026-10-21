#include "World/Actor/Movement/Systems/PlayerLiquidContactSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void PlayerLiquidContactSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &contact = context.contact;
    auto &travel = context.travel;

    contact.liquid = LiquidBlocksFetch::at(context.level, state.start, (abilities.swimming || abilities.gliding || travel.pose.mCrawling),
                                            travel.pose.mHeight);
    contact.lavaTravel = !abilities.flying && contact.liquid.lava;
    contact.waterTravel = !abilities.flying && !contact.lavaTravel && contact.liquid.water;
}
