#pragma once

#include "Actor/ActorCategory.h"
#include "Actor/ActorSize.h"
#include "Actor/ServerActor.h"
#include "Server/PropertiesSettings.h"

#include <string>

class Level;
class LootTable;
class ServerNetworkHandler;
class ServerPlayer;

class MobActor : public ServerActor {
public:
    MobActor(uint64_t runtimeId, const std::string &identifier);

    virtual ActorCategory getCategory() const = 0;

    ActorSize getSize() const override = 0;

    virtual float getDefaultMaxHealth() const = 0;

    virtual int getExperienceDrop() const { return 0; }

    virtual const LootTable *getLootTable() const;

    virtual float resolveMaxHealth(Difficulty difficulty) const;

    virtual void finalizeSpawn() {
    }

    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    void applyDefaults(Difficulty difficulty);

    static int randomRange(int minimum, int maximum);

    void dropLoot(ServerNetworkHandler &owner, Level &level, const ServerPlayer *killer, int32_t lootingLevel) const;
};
