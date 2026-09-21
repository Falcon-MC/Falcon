#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class AbstractSlimeActor : public HostileActor {
public:
    static constexpr int LARGE_SIZE = 4;

    static constexpr int MEDIUM_SIZE = 2;

    static constexpr int SMALL_SIZE = 1;

    static constexpr float SIZE_SCALE = 0.52f;

    using HostileActor::HostileActor;

    int getSizeVariant() const { return mSizeVariant; }

    void setSizeVariant(int variant);

    ActorSize getSize() const override;

    float getDefaultMaxHealth() const override;

    int getExperienceDrop() const override { return mSizeVariant; }

    virtual float getContactDamage() const = 0;

    void finalizeSpawn() override;

    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    void tick(ServerNetworkHandler &owner) override;

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

private:
    void _split(ServerNetworkHandler &owner, Level &level);

    void _attackTouchingPlayers(ServerNetworkHandler &owner);

    int mSizeVariant = LARGE_SIZE;
    int32_t mAttackCooldown = 0;
};
