#pragma once

#include "Actor/ActorCategory.h"
#include "Actor/ActorSizeTable.h"
#include "Actor/Mob/MobLoot.h"
#include "Actor/ServerActor.h"
#include "Server/PropertiesSettings.h"

#include <string>
#include <vector>

class MobActor : public ServerActor {
public:
    MobActor(uint64_t runtimeId, const std::string &identifier);

    virtual ActorCategory getCategory() const = 0;

    virtual ActorSize getSize() const = 0;

    virtual float getDefaultMaxHealth() const = 0;

    virtual int getExperienceDrop() const { return 0; }

    virtual const std::vector<LootEntry> &getLootEntries() const;

    // Health of the mob once the difficulty and the instance variations are taken into account.
    virtual float resolveMaxHealth(Difficulty difficulty) const;

    void applyDefaults(Difficulty difficulty);

    static int randomRange(int minimum, int maximum);

    std::vector<MobDrop> rollDrops(bool onFire, int32_t lootingLevel) const;
};
