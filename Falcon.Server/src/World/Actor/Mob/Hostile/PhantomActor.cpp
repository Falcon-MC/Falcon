#include "World/Actor/Mob/Hostile/PhantomActor.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"

#include <cmath>
#include <random>

FALCON_REGISTER_ACTOR(PhantomActor, PhantomActor::IDENTIFIER);

namespace {
    // minecraft:insomnia days_until_insomnia of player.json, in ticks
    const int32_t INSOMNIA_TICKS = 3 * 24000;
    const float PLAYER_REACH_SQUARED = 100.0f * 100.0f;
    const float CROWD_RANGE = 48.0f;
    // more phantoms than this around the spot calls the spawn off
    const int32_t CROWD_LIMIT = 4;

    int32_t nextInt(std::mt19937 &random, int32_t bound) {
        return bound <= 0 ? 0 : std::uniform_int_distribution<int32_t>(0, bound - 1)(random);
    }

    bool underOpenSky(Level &level, const Vector3f &position) {
        const int32_t x = (int32_t) std::floor(position.x);
        const int32_t y = (int32_t) std::floor(position.y);
        const int32_t z = (int32_t) std::floor(position.z);
        if (y >= level.getMaxY())
            return true;
        if (y < level.getMinY())
            return false;

        LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
        return chunk != nullptr && y >= chunk->getHeight(x & 15, z & 15);
    }

    int32_t phantomsAround(ServerNetworkHandler &owner, Level &level, const Vector3f &center) {
        int32_t count = 0;
        owner.forEachActorNear(center, CROWD_RANGE * 1.75f, [&](ServerActor &actor) {
            const Vector3f &position = actor.getPosition();
            if (actor.isAlive() && actor.isIn(level) && dynamic_cast<PhantomActor *>(&actor) != nullptr
                && std::fabs(position.x - center.x) <= CROWD_RANGE && std::fabs(position.y - center.y) <= CROWD_RANGE
                && std::fabs(position.z - center.z) <= CROWD_RANGE)
                ++count;
        });
        return count;
    }
}

bool PhantomActor::placeNaturalSpawn(ServerNetworkHandler &owner, Level &level, Vector3f &position,
                                     std::mt19937 &random) const {
    if (!level.hasSkyLight())
        return false;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !player.isAlive() || &owner.getLevelFor(player) != &level)
            continue;

        const Vector3f &at = player.getPosition();
        const float dx = position.x - at.x;
        const float dy = position.y - at.y;
        const float dz = position.z - at.z;
        if (dx * dx + dy * dy + dz * dz >= PLAYER_REACH_SQUARED || !underOpenSky(level, at))
            continue;

        // the longer awake, the likelier: no chance before three days
        if (nextInt(random, player.getTimeSinceRest()) < INSOMNIA_TICKS)
            continue;

        const int32_t offsetX = nextInt(random, 21) - 10;
        const int32_t offsetY = nextInt(random, 15) + 20;
        const int32_t offsetZ = nextInt(random, 21) - 10;
        const Vector3f above(std::floor(at.x + (float) offsetX), std::floor(at.y + (float) offsetY),
                             std::floor(at.z + (float) offsetZ));
        if (phantomsAround(owner, level, above) > CROWD_LIMIT)
            continue;

        position = above;
        return true;
    }
    return false;
}
