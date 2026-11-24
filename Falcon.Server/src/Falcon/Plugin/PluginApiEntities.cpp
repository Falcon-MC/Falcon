#include "Falcon/Plugin/PluginServerApi.h"

#include "World/Actor/DamageCause.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginActor.h"
#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Falcon/Plugin/PluginEvents.h"

#include <cmath>
#include <string>

using namespace PluginApiHelpers;

namespace {
    FalconVec3 toVec3(const Vector3f &value) {
        return FalconVec3{(double) value.x, (double) value.y, (double) value.z};
    }

    Vector3f toVector3f(const FalconVec3 &value) {
        return Vector3f((float) value.x, (float) value.y, (float) value.z);
    }

    bool isFinite(const Vector3f &value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    ServerPlayer *asPlayer(Actor *value) {
        return dynamic_cast<ServerPlayer *>(value);
    }

    ServerActor *asActor(Actor *value) {
        return dynamic_cast<ServerActor *>(value);
    }

    std::string deathMessageKeyFor(const char *cause, Actor *attacker) {
        if (cause != nullptr) {
            const char *key = DamageCause::findDeathMessageKey(cause);
            if (key != nullptr)
                return key;
            if (std::string(cause).rfind("death.", 0) == 0)
                return cause;
        }

        if (attacker == nullptr)
            return "death.attack.generic";

        return asPlayer(attacker) != nullptr ? "death.attack.player" : "death.attack.mob";
    }

    FalconEntity *playerEntity(FalconPlayer *target) {
        return toHandle(static_cast<Actor *>(player(target)));
    }

    FalconPlayer *entityPlayer(FalconEntity *target) {
        return toHandle(asPlayer(entity(target)));
    }

    const char *entityType(FalconEntity *target) {
        return hold(entity(target)->getIdentifier());
    }

    uint64_t entityRuntimeId(FalconEntity *target) {
        return entity(target)->getRuntimeId();
    }

    FalconLevel *entityLevel(FalconEntity *target) {
        return toHandle(&owner().getLevelFor(*entity(target)));
    }

    FalconVec3 entityPosition(FalconEntity *target) {
        return toVec3(entity(target)->getPosition());
    }

    FalconVec3 entityRotation(FalconEntity *target) {
        return toVec3(entity(target)->getRotation());
    }

    // a player first: players keep their own runtime id index
    FalconEntity *serverEntity(uint64_t runtimeId) {
        if (ServerPlayer *player = owner().getPlayerByRuntimeId(runtimeId))
            return toHandle(static_cast<Actor *>(player));
        if (ServerActor *actor = owner().getActorByRuntimeId(runtimeId))
            return toHandle(static_cast<Actor *>(actor));
        for (const auto &item : owner().getItemEntities()) {
            if (!item->isRemoved() && item->getRuntimeId() == runtimeId)
                return toHandle(static_cast<Actor *>(item.get()));
        }
        return nullptr;
    }

    int entityTeleportWithRotation(FalconEntity *target, FalconLevel *destination, FalconVec3 position,
                                   const FalconVec3 *rotation) {
        if (target == nullptr)
            return 0;
        Actor *value = entity(target);
        Vector3f to = toVector3f(position);
        const Vector3f facing = rotation == nullptr ? value->getRotation() : toVector3f(*rotation);
        if (!isFinite(to) || !isFinite(facing))
            return 0;

        Level *destinationLevel = destination == nullptr ? &owner().getLevelFor(*value) : level(destination);
        // the plugin drawing it keeps it where its own copy is: that is no teleport to tell anyone of
        const ServerActor *drawn = asActor(value);
        if ((drawn == nullptr || !drawn->isDrawnByPlugin())
            && !PluginEvents::teleport(*value, to, "plugin", &destinationLevel))
            return 0;
        if (!isFinite(to))
            return 0;

        if (ServerPlayer *playerValue = asPlayer(value)) {
            if (!playerValue->isIn(*destinationLevel)) {
                owner().changePlayerLevel(*playerValue, *destinationLevel, to);
                if (!playerValue->isIn(*destinationLevel))
                    return 0;
                if (rotation != nullptr)
                    playerValue->setRotation(facing);
                return 1;
            }
            if (rotation != nullptr)
                playerValue->setRotation(facing);
            playerValue->teleport(owner(), to);
            return 1;
        }

        ServerActor *actorValue = asActor(value);
        if (actorValue == nullptr)
            return 0;

        if (rotation != nullptr)
            actorValue->setRotation(facing);
        if (!actorValue->isIn(*destinationLevel)) {
            owner().changeActorLevel(*actorValue, *destinationLevel, to);
            return actorValue->isIn(*destinationLevel);
        }

        actorValue->teleport(to);
        owner().broadcastActorMove(*actorValue);
        return 1;
    }

    void entityTeleport(FalconEntity *target, FalconLevel *destination, FalconVec3 position) {
        entityTeleportWithRotation(target, destination, position, nullptr);
    }

    FalconVec3 entityMotion(FalconEntity *target) {
        return toVec3(entity(target)->getMotion());
    }

    void entitySetMotion(FalconEntity *target, FalconVec3 motion) {
        Actor *value = entity(target);
        value->setMotion(toVector3f(motion));
        owner().sendActorMotion(*value);
    }

    float entityHealth(FalconEntity *target) {
        return entity(target)->getHealth();
    }

    float entityMaxHealth(FalconEntity *target) {
        return entity(target)->getMaxHealth();
    }

    void entitySetHealth(FalconEntity *target, float health) {
        Actor *value = entity(target);
        value->setHealth(health);

        if (ServerPlayer *playerValue = asPlayer(value)) {
            owner()._sendHealth(*playerValue);
            return;
        }

        if (ServerActor *actorValue = asActor(value))
            owner().syncActorAttributes(*actorValue);
    }

    int entityIsAlive(FalconEntity *target) {
        return entity(target)->isAlive() ? 1 : 0;
    }

    int entityDamage(FalconEntity *target, float amount, const char *cause, FalconEntity *attacker) {
        Actor *value = entity(target);
        Actor *source = attacker == nullptr ? nullptr : entity(attacker);

        if (ServerPlayer *playerValue = asPlayer(value)) {
            const std::string key = deathMessageKeyFor(cause, source);
            if (source == nullptr) {
                const ActorDamageSource damage = ActorDamageSource::environment(key, playerValue->getName());
                return owner().hurt(*playerValue, amount, damage) == DamageResult::Dealt ? 1 : 0;
            }

            const ActorDamageSource damage = ActorDamageSource::attack(key, playerValue->getName(), *source,
                                                                       source->getName(), source->getPosition());
            return owner().hurt(*playerValue, amount, damage) == DamageResult::Dealt ? 1 : 0;
        }

        ServerActor *actorValue = asActor(value);
        if (actorValue == nullptr)
            return 0;

        return owner().damageActor(*actorValue, amount, source) ? 1 : 0;
    }

    void entityKill(FalconEntity *target) {
        Actor *value = entity(target);

        if (ServerPlayer *playerValue = asPlayer(value)) {
            owner().killPlayer(*playerValue, "death.attack.generic", {playerValue->getName()});
            return;
        }

        ServerActor *actorValue = asActor(value);
        if (actorValue != nullptr && !actorValue->isDead())
            actorValue->kill(owner(), nullptr, 0);
    }

    void entityRemove(FalconEntity *target) {
        Actor *value = entity(target);
        if (auto *item = dynamic_cast<ItemActor *>(value)) {
            item->setRemoved(true);
            return;
        }
        ServerActor *actorValue = asActor(value);
        if (actorValue == nullptr)
            return;

        const int64_t id = actorValue->getUniqueId();
        owner().postToMainThread([id] { owner().removeActor(id); });
    }

    const char *entityNameTag(FalconEntity *target) {
        Actor *value = entity(target);

        if (ServerActor *actorValue = asActor(value))
            return hold(actorValue->getNameTag());

        return hold(value->getName());
    }

    void entitySetNameTag(FalconEntity *target, const char *nameTag) {
        ServerActor *actorValue = asActor(entity(target));
        if (actorValue == nullptr)
            return;

        actorValue->setNameTag(nameTag == nullptr ? std::string() : std::string(nameTag));
        owner().sendActorNameTag(*actorValue);
    }

    int entityIsOnFire(FalconEntity *target) {
        return entity(target)->isOnFire() ? 1 : 0;
    }

    int entityIsInvisible(FalconEntity *target) {
        return entity(target)->getFlags().get(ActorFlag::Invisible) ? 1 : 0;
    }

    void entitySetInvisible(FalconEntity *target, int invisible) {
        entity(target)->getFlags().set(ActorFlag::Invisible, invisible != 0);
    }

    void entitySetOnFire(FalconEntity *target, uint32_t ticks) {
        Actor *value = entity(target);
        if (ticks == 0)
            value->extinguish();
        else
            value->setFireTicks(ticks > 32767u ? 32767 : (int) ticks);

        if (ServerPlayer *playerValue = asPlayer(value)) {
            owner()._sendEntityData(*playerValue);
            return;
        }

        if (ServerActor *actorValue = asActor(value))
            owner().syncActorFlags(*actorValue);
    }

    uint32_t levelEntityCount(FalconLevel *handle) {
        const Level &target = *level(handle);
        uint32_t count = 0;

        for (auto &entry: owner().getActors()) {
            if (entry.second->isIn(target))
                count++;
        }

        for (auto &entry: owner().getPlayers()) {
            if (entry.second.isSpawned() && entry.second.isIn(target))
                count++;
        }

        for (const auto &item : owner().getItemEntities()) {
            if (!item->isRemoved() && item->isIn(target))
                count++;
        }

        return count;
    }

    FalconEntity *levelEntity(FalconLevel *handle, uint32_t index) {
        const Level &target = *level(handle);
        uint32_t current = 0;

        for (auto &entry: owner().getActors()) {
            if (!entry.second->isIn(target))
                continue;
            if (current == index)
                return toHandle(static_cast<Actor *>(entry.second.get()));
            current++;
        }

        for (auto &entry: owner().getPlayers()) {
            if (!entry.second.isSpawned() || !entry.second.isIn(target))
                continue;
            if (current == index)
                return toHandle(static_cast<Actor *>(&entry.second));
            current++;
        }

        for (const auto &item : owner().getItemEntities()) {
            if (item->isRemoved() || !item->isIn(target))
                continue;
            if (current == index)
                return toHandle(static_cast<Actor *>(item.get()));
            current++;
        }

        return nullptr;
    }

    FalconItem *itemEntityItem(FalconEntity *target) {
        auto *item = dynamic_cast<ItemActor *>(entity(target));
        return item == nullptr ? nullptr : toHandle(new ItemStack(item->getItem()));
    }

    int32_t itemEntityPickupDelay(FalconEntity *target) {
        auto *item = dynamic_cast<ItemActor *>(entity(target));
        return item == nullptr ? 0 : item->getPickupDelay();
    }

    void itemEntitySetPickupDelay(FalconEntity *target, int32_t pickupDelay) {
        if (auto *item = dynamic_cast<ItemActor *>(entity(target)))
            item->setPickupDelay(pickupDelay);
    }
}

namespace {
    PluginActor *asPluginActor(FalconEntity *target) {
        return target == nullptr ? nullptr : dynamic_cast<PluginActor *>(entity(target));
    }

    int entitySetPluginDrawn(FalconEntity *target, int pluginDrawn) {
        PluginActor *actor = asPluginActor(target);
        if (actor == nullptr)
            return 0;
        if (actor->isDrawnByPlugin() == (pluginDrawn != 0))
            return 1;
        // whoever saw it drawn by Falcon forgets it; the next visibility pass shows it again when Falcon draws it
        if (pluginDrawn != 0)
            owner().broadcastActorRemove(*actor);
        actor->setDrawnByPlugin(pluginDrawn != 0);
        return 1;
    }

    int entityIsPluginDrawn(FalconEntity *target) {
        const PluginActor *actor = asPluginActor(target);
        return actor != nullptr && actor->isDrawnByPlugin() ? 1 : 0;
    }

    int entitySetSize(FalconEntity *target, float width, float height) {
        PluginActor *actor = asPluginActor(target);
        if (actor == nullptr || !(width > 0.0f) || !(height > 0.0f) || !std::isfinite(width) || !std::isfinite(height))
            return 0;
        actor->setSize(width, height);
        return 1;
    }
}

void PluginServerApi::fillEntities(FalconServerApi &api) {
    api.entitySetPluginDrawn = &entitySetPluginDrawn;
    api.entityIsPluginDrawn = &entityIsPluginDrawn;
    api.entitySetSize = &entitySetSize;
    api.playerEntity = &playerEntity;
    api.entityPlayer = &entityPlayer;
    api.entityType = &entityType;
    api.entityRuntimeId = &entityRuntimeId;
    api.serverEntity = &serverEntity;
    api.entityLevel = &entityLevel;
    api.entityPosition = &entityPosition;
    api.entityRotation = &entityRotation;
    api.entityTeleport = &entityTeleport;
    api.entityTeleportWithRotation = &entityTeleportWithRotation;
    api.entityMotion = &entityMotion;
    api.entitySetMotion = &entitySetMotion;
    api.entityHealth = &entityHealth;
    api.entityMaxHealth = &entityMaxHealth;
    api.entitySetHealth = &entitySetHealth;
    api.entityIsAlive = &entityIsAlive;
    api.entityDamage = &entityDamage;
    api.entityKill = &entityKill;
    api.entityRemove = &entityRemove;
    api.entityNameTag = &entityNameTag;
    api.entitySetNameTag = &entitySetNameTag;
    api.entityIsOnFire = &entityIsOnFire;
    api.entitySetOnFire = &entitySetOnFire;
    api.entityIsInvisible = &entityIsInvisible;
    api.entitySetInvisible = &entitySetInvisible;
    api.levelEntityCount = &levelEntityCount;
    api.levelEntity = &levelEntity;
    api.itemEntityItem = &itemEntityItem;
    api.itemEntityPickupDelay = &itemEntityPickupDelay;
    api.itemEntitySetPickupDelay = &itemEntitySetPickupDelay;
}
