#include "World/Actor/Movement/Systems/WaterSinkInputSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void WaterSinkInputSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &contact = context.contact;

    if (contact.waterTravel || contact.lavaTravel) {
        if (input.pressingSneak || MovementPhysics::input(input.packet, PlayerAuthInputData::Descend))
            state.velocity.y -= 0.04f;
    }
}
