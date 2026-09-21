#include "Actor/Mob/MobActor.h"

#include "Actor/ServerPlayer.h"
#include "Level/Level.h"
#include "Loot/LootItems.h"
#include "Loot/LootTableRegistry.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <random>

namespace {
    std::mt19937 &experienceRandom() {
        static std::mt19937 generator(0x9E3779B9u);
        return generator;
    }

    std::mt19937 &lootRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

MobActor::MobActor(uint64_t runtimeId, const std::string &identifier) : ServerActor(runtimeId, identifier) {
}

float MobActor::resolveMaxHealth(Difficulty difficulty) const {
    (void) difficulty;
    return getDefaultMaxHealth();
}

void MobActor::applyDefaults(Difficulty difficulty) {
    const float health = resolveMaxHealth(difficulty);
    setMaxHealth(health);
    setHealth(health);
}

const LootTable *MobActor::getLootTable() const {
    return LootTableRegistry::getInstance().getForEntity(mIdentifier);
}

int MobActor::randomRange(int minimum, int maximum) {
    if (minimum >= maximum)
        return minimum;

    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(experienceRandom());
}

void MobActor::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t lootingLevel) {
    if (isDead())
        return;

    if (owner.getLevel().getGameRules().getBool("domobloot")) {
        Level &level = owner.getLevelFor(*this);
        dropLoot(owner, level, source, lootingLevel);

        const int experience = getExperienceDrop();
        if (experience > 0 && source != nullptr)
            owner.spawnExperienceOrbs(level, getPosition(), experience);
    }

    ServerActor::kill(owner, source, lootingLevel);
}

void MobActor::dropLoot(ServerNetworkHandler &owner, Level &level, const ServerPlayer *killer,
                        int32_t lootingLevel) const {
    const LootTable *table = getLootTable();
    if (table == nullptr)
        return;

    LootContext context(lootRandom());
    context.mLootingLevel = lootingLevel;
    context.mDifficulty = (int32_t) owner.getProperties().getDifficulty();
    context.mRegionalDifficulty = level.getRegionalDifficulty(context.mDifficulty);
    context.mKilledByPlayer = killer != nullptr;
    context.mOnFire = isOnFire();
    if (killer != nullptr)
        context.mKillerIdentifier = killer->getIdentifier();

    const Vector3f position = getPosition();
    for (const LootDrop &drop: table->roll(context)) {
        const ItemStack stack = LootItems::toItemStack(owner, drop);
        if (!stack.isAir())
            owner.dropItem(level, position, stack, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    }
}
