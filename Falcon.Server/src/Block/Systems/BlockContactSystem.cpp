#include "Block/Systems/BlockContactSystem.h"

#include "Actor/ServerPlayer.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const float CONTACT_INSET = 0.3f;
    const float PLAYER_CONTACT_HEIGHT = 1.8f;
    const float STEP_PROBE_DEPTH = 0.01f;
}

void BlockContactSystem::tick(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.isSpawned() || player.isDead() || player.isAwaitingDimensionAck())
        return;

    if (player.getPortalCooldown() > 0)
        player.setPortalCooldown(player.getPortalCooldown() - 1);

    Level &level = owner.getLevelFor(player);
    const Vector3f position = player.getPosition();

    const int32_t minX = (int32_t) std::floor(position.x - CONTACT_INSET);
    const int32_t maxX = (int32_t) std::floor(position.x + CONTACT_INSET);
    const int32_t minZ = (int32_t) std::floor(position.z - CONTACT_INSET);
    const int32_t maxZ = (int32_t) std::floor(position.z + CONTACT_INSET);
    const int32_t minY = (int32_t) std::floor(position.y);
    const int32_t maxY = (int32_t) std::floor(position.y + PLAYER_CONTACT_HEIGHT);

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

                block->onActorInside(owner, player, Vector3i(x, y, z), *state);

                if (&owner.getLevelFor(player) != &level || player.isAwaitingDimensionAck())
                    return;
            }
        }
    }

    if (player.isOnGround()) {
        const Vector3i below((int32_t) std::floor(position.x), (int32_t) std::floor(position.y - STEP_PROBE_DEPTH),
                             (int32_t) std::floor(position.z));
        const BlockState *state = level.isChunkResident(below.x >> 4, below.z >> 4)
                                  ? level.peekBlockPtr(below.x, below.y, below.z)
                                  : nullptr;
        const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
        if (block != nullptr)
            block->onStepOn(owner, player, below, *state);
    }

    if (player.getLastPortalTick() != owner.getCurrentTick())
        player.setPortalTicks(0);
}
