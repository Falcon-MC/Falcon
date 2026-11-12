#include "World/Actor/Movement/Systems/PlayerFlightSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

#include "World/Level/Block/Block.h"

#include <algorithm>
#include <cmath>

using namespace MovementPhysics;

void PlayerFlightSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &travel = context.travel;

    if (abilities.flying) {
        // fly speed advertised by LoginHandler; the game doubles it when sprinting
        travel.acceleration = FLY_SPEED * (abilities.sprinting ? 2.0f : 1.0f);
        if (state.onGround) {
            const Block *ground = blockAt(context.level, state.start.x, state.start.y - GROUND_PROBE_DEPTH,
                                          state.start.z);
            travel.friction *= ground == nullptr ? DEFAULT_BLOCK_FRICTION : ground->getFrictionFactor();
        }
    }
    if (!abilities.flying && !abilities.flightControls)
        return;

    // the vertical controls follow the flight state the input asks for: on with the input that starts flight, whose
    // jump and sneak keys stand for the fly flags, off with the one that stops it
    const PlayerAuthInputPacket &packet = input.getPacket();
    const bool controls = abilities.flightControls;
    const bool ascend = controls && (MovementPhysics::input(packet, PlayerAuthInputData::WantUp)
                                     || (abilities.startsFlight && input.jumpHeld));
    const bool descend = controls
                         && (MovementPhysics::input(packet, PlayerAuthInputData::WantDown)
                             || (abilities.startsFlight
                                 && MovementPhysics::input(packet, PlayerAuthInputData::Sneaking)));
    const bool ascendSlow = controls && MovementPhysics::input(packet, PlayerAuthInputData::WantUpSlow);
    const bool descendSlow = controls && MovementPhysics::input(packet, PlayerAuthInputData::WantDownSlow);
    // VerticalFlySpeedControlSystem also sets FrictionModifierComponent for idle flight
    if (std::max(std::fabs(input.sideways), std::fabs(input.forward)) < 0.01f) {
        const bool mouse = packet.mInputMode == PlayerInputMode::Mouse;
        if (abilities.flying)
            travel.frictionModifier = mouse ? 0.375f : 0.75f;
        if (mouse && !ascend && !descend && !ascendSlow && !descendSlow)
            state.velocity.y *= 0.375f;
    }
    if (ascend && descend) {
        state.velocity.y = 0.0f;
    } else {
        state.velocity.y += VERTICAL_FLY_SPEED
                            * ((ascend ? 0.15f : 0.0f) + (ascendSlow ? 0.05f : 0.0f) - (descend ? 0.22f : 0.0f)
                               - (descendSlow ? 0.15f : 0.0f));
    }
}
