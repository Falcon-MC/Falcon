#include "World/Actor/Movement/Systems/WaterSinkInputSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void WaterSinkInputSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &contact = context.contact;

    if (context.waterTravel.active || context.liquidTravel.lava) {
        if (MovementPhysics::input(input.getPacket(), PlayerAuthInputData::WantDownSlow)
            || MovementPhysics::input(input.getPacket(), PlayerAuthInputData::WantDown))
            state.velocity.y -= 0.04f;
    }
}
