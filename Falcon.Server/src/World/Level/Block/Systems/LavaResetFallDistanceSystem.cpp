#include "World/Level/Block/Systems/LavaResetFallDistanceSystem.h"

#include "World/Actor/Player/ServerPlayer.h"

void LavaResetFallDistanceSystem::tick(ServerPlayer &player, const LiquidContact &contact) {
    if (contact.lava)
        player.resetFallDistance();
}
