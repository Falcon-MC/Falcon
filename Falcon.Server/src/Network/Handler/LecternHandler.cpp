#include "Network/Handler/LecternHandler.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LecternUpdatePacket.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/LecternBlock.h"
#include "World/Level/Level.h"

#include <cmath>

namespace {
    // how far a player may turn pages from, measured from the block's corner as the game does
    const float SURVIVAL_REACH = 8.7f;
    const float CREATIVE_REACH = 14.0f;
}

void LecternHandler::handleUpdate(ServerNetworkHandler &owner, ServerPlayer &player, const LecternUpdatePacket &packet) {
    Level &level = owner.getLevelFor(player);
    const Vector3i &position = packet.mBlockPosition;
    LecternBlockActor *lectern = level.getBlockActors().find<LecternBlockActor>(position);
    if (lectern == nullptr || !lectern->hasBook())
        return;

    const Vector3f &from = player.getPosition();
    const float dx = from.x - (float) position.x, dy = from.y - (float) position.y, dz = from.z - (float) position.z;
    const float reach = player.isCreative() ? CREATIVE_REACH : SURVIVAL_REACH;
    if (!(std::sqrt(dx * dx + dy * dy + dz * dz) <= reach))
        return;

    const int page = packet.mPage;
    if (page < 0 || page >= lectern->mTotalPages || page == lectern->mPage)
        return;

    // the page moves and the lectern pulses
    lectern->mPage = page;
    LecternBlock::pulse(level, position);
    BlockActionHandler::broadcastBlockActorData(owner, level, *lectern);
}
