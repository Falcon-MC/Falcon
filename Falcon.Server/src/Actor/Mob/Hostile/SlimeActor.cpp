#include "Actor/Mob/Hostile/SlimeActor.h"

#include "Actor/ActorClassRegistry.h"
#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"
#include "Level/Level.h"

namespace {
    const int32_t SLIME_CHUNK_MAX_Y = 40;
    const int32_t SLIME_CHUNK_CHANCE = 10;
    const uint32_t SLIME_CHUNK_X_MULTIPLIER = 0x1f1f1f1fu;
    const int32_t SWAMP_MIN_Y = 51;
    const int32_t SWAMP_MAX_Y = 69;
    const int32_t SWAMP_MAX_LIGHT = 7;
    const float SWAMP_CHANCE = 0.5f;
    const char *const SWAMP_TAGS[] = {"swamp", "mangrove_swamp"};

    bool isSwamp(int32_t biomeId) {
        for (const char *tag: SWAMP_TAGS) {
            if (BiomeChunkGenDataRegistry::hasTag(biomeId, tag))
                return true;
        }
        return false;
    }
}

bool SlimeActor::isSlimeChunk(int32_t chunkX, int32_t chunkZ) {
    std::mt19937 generator(((uint32_t) chunkX * SLIME_CHUNK_X_MULTIPLIER) ^ (uint32_t) chunkZ);
    return generator() % SLIME_CHUNK_CHANCE == 0;
}

bool SlimeActor::canSpawnNaturally(Level &level, const Vector3i &position, int32_t biomeId, int32_t light,
                                   std::mt19937 &random) const {
    if (position.y < SLIME_CHUNK_MAX_Y && isSlimeChunk(position.x >> 4, position.z >> 4))
        return true;

    if (!isSwamp(biomeId) || position.y < SWAMP_MIN_Y || position.y > SWAMP_MAX_Y)
        return false;

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    return unit(random) < SWAMP_CHANCE && unit(random) < level.getMoonBrightness()
           && light <= std::uniform_int_distribution<int32_t>(0, SWAMP_MAX_LIGHT)(random);
}

float SlimeActor::getContactDamage() const {
    if (getSizeVariant() == LARGE_SIZE)
        return 4.0f;
    if (getSizeVariant() == MEDIUM_SIZE)
        return 2.0f;
    return 0.0f;
}

const LootTable *SlimeActor::getLootTable() const {
    return getSizeVariant() == SMALL_SIZE ? AbstractSlimeActor::getLootTable() : nullptr;
}

FALCON_REGISTER_ACTOR(SlimeActor, SlimeActor::IDENTIFIER);
