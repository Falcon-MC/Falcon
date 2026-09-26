#pragma once

#include "Actor/Mob/Hostile/AbstractSlimeActor.h"

class SlimeActor : public AbstractSlimeActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:slime";

    using AbstractSlimeActor::AbstractSlimeActor;

    float getContactDamage() const override;

    const LootTable *getLootTable() const override;

    bool canSpawnNaturally(Level &level, const Vector3i &position, int32_t biomeId, int32_t light,
                           std::mt19937 &random) const override;

    static bool isSlimeChunk(int32_t chunkX, int32_t chunkZ);
};
