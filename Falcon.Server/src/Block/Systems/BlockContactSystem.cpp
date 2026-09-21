#include "Block/Systems/BlockContactSystem.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const float PLAYER_CONTACT_WIDTH = 0.6f;
    const float PLAYER_CONTACT_HEIGHT = 1.8f;
    const float STEP_PROBE_DEPTH = 0.01f;
}

void BlockContactSystem::tick(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.isSpawned() || player.isDead() || player.isAwaitingDimensionAck())
        return;

    if (player.getPortalCooldown() > 0)
        player.setPortalCooldown(player.getPortalCooldown() - 1);

    if (!touchBlocks(owner, player, ActorSize{PLAYER_CONTACT_WIDTH, PLAYER_CONTACT_HEIGHT}))
        return;

    if (player.getLastPortalTick() != owner.getCurrentTick())
        player.setPortalTicks(0);
}

void BlockContactSystem::tick(ServerNetworkHandler &owner, ServerActor &actor) {
    if (!actor.isAlive() || actor.isProjectile())
        return;

    touchBlocks(owner, actor, actor.getSize());
}

bool BlockContactSystem::touchBlocks(ServerNetworkHandler &owner, Actor &actor, const ActorSize &size) {
    Level &level = owner.getLevelFor(actor);
    const Vector3f position = actor.getPosition();
    const float inset = size.mWidth * 0.5f;

    const int32_t minX = (int32_t) std::floor(position.x - inset);
    const int32_t maxX = (int32_t) std::floor(position.x + inset);
    const int32_t minZ = (int32_t) std::floor(position.z - inset);
    const int32_t maxZ = (int32_t) std::floor(position.z + inset);
    const int32_t minY = (int32_t) std::floor(position.y);
    const int32_t maxY = (int32_t) std::floor(position.y + size.mHeight);

    for (int32_t x = minX; x <= maxX; ++x) {
        for (int32_t y = minY; y <= maxY; ++y) {
            for (int32_t z = minZ; z <= maxZ; ++z) {
                if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                    continue;

                if (!level.isChunkResident(x >> 4, z >> 4))
                    continue;

                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr || state->mName == "minecraft:air")
                    continue;

                const Block *block = VanillaBlocks::fromIdentifier(state->mName);
                if (block == nullptr)
                    continue;

                block->onActorInside(owner, actor, Vector3i(x, y, z), *state);

                if (&owner.getLevelFor(actor) != &level || !actor.isAlive())
                    return false;

                const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(&actor);
                if (player != nullptr && player->isAwaitingDimensionAck())
                    return false;
            }
        }
    }

    if (!actor.isOnGround())
        return true;

    const Vector3i below((int32_t) std::floor(position.x), (int32_t) std::floor(position.y - STEP_PROBE_DEPTH),
                         (int32_t) std::floor(position.z));
    if (!level.isChunkResident(below.x >> 4, below.z >> 4))
        return true;

    const BlockState *state = level.peekBlockPtr(below.x, below.y, below.z);
    const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
    if (block != nullptr)
        block->onStepOn(owner, actor, below, *state);

    return true;
}
