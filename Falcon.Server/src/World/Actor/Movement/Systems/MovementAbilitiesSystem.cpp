#include "World/Actor/Movement/Systems/MovementAbilitiesSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ElytraItem.h"
#include "Protocol/Types/StartGameTypes.h"

using namespace MovementPhysics;

void MovementAbilitiesSystem::tick(PlayerMovementContext &context) {
    auto &abilities = context.abilities;

    abilities.gameType = context.player.getGameType();
    abilities.spectator = abilities.gameType == (int32_t) GameType::Spectator;
    abilities.flying = context.player.isFlying() || abilities.spectator;
    abilities.gliding = !abilities.flying && ElytraItem::isWearingElytra(context.player)
                         && activeState(context.player.getFlags().get(ActorFlag::Gliding), context.input.packet,
                                        PlayerAuthInputData::StartGliding, PlayerAuthInputData::StopGliding);
    abilities.swimming = !abilities.flying && activeState(context.player.getFlags().get(ActorFlag::Swimming), context.input.packet,
                                                PlayerAuthInputData::StartSwimming, PlayerAuthInputData::StopSwimming);
}
