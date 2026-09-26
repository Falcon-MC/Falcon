#include "Plugin/PluginServerApi.h"

#include "Actor/DamageCause.h"
#include "Actor/ActorDamageSource.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginApiHelpers.h"

#include <string>

using namespace PluginApiHelpers;

namespace {
    FalconVec3 toVec3(const Vector3f &value) {
        return FalconVec3{(double) value.x, (double) value.y, (double) value.z};
    }

    Vector3f toVector3f(const FalconVec3 &value) {
        return Vector3f((float) value.x, (float) value.y, (float) value.z);
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

    void entityTeleport(FalconEntity *target, FalconLevel *destination, FalconVec3 position) {
        Actor *value = entity(target);
        const Vector3f to = toVector3f(position);
        const DimensionType dimension =
                destination == nullptr ? value->getDimension() : level(destination)->getDimensionType();

        if (ServerPlayer *playerValue = asPlayer(value)) {
            if (playerValue->getDimension() != dimension)
                owner().changePlayerDimension(*playerValue, dimension, to);
            else
                playerValue->teleport(owner(), to);
            return;
        }

        ServerActor *actorValue = asActor(value);
        if (actorValue == nullptr)
            return;

        if (actorValue->getDimension() != dimension) {
            owner().changeActorDimension(*actorValue, dimension, to);
            return;
        }

        actorValue->teleport(to);
        owner().broadcastActorMove(*actorValue);
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

            const ActorDamageSource damage = ActorDamageSource::attack(key, playerValue->getName(), *source, source->getName(),
                                                             source->getPosition());
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
        ServerActor *actorValue = asActor(entity(target));
        if (actorValue == nullptr)
            return;

        const int64_t id = actorValue->getUniqueId();
        owner().postToMainThread([id] {
            owner().removeActor(id);
        });
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
        const DimensionType dimension = level(handle)->getDimensionType();
        uint32_t count = 0;

        for (auto &entry: owner().getActors()) {
            if (entry.second->getDimension() == dimension)
                count++;
        }

        for (auto &entry: owner().getPlayers()) {
            if (entry.second.isSpawned() && entry.second.getDimension() == dimension)
                count++;
        }

        return count;
    }

    FalconEntity *levelEntity(FalconLevel *handle, uint32_t index) {
        const DimensionType dimension = level(handle)->getDimensionType();
        uint32_t current = 0;

        for (auto &entry: owner().getActors()) {
            if (entry.second->getDimension() != dimension)
                continue;
            if (current == index)
                return toHandle(static_cast<Actor *>(entry.second.get()));
            current++;
        }

        for (auto &entry: owner().getPlayers()) {
            if (!entry.second.isSpawned() || entry.second.getDimension() != dimension)
                continue;
            if (current == index)
                return toHandle(static_cast<Actor *>(&entry.second));
            current++;
        }

        return nullptr;
    }
}

void PluginServerApi::fillEntities(FalconServerApi &api) {
    api.playerEntity = &playerEntity;
    api.entityPlayer = &entityPlayer;
    api.entityType = &entityType;
    api.entityRuntimeId = &entityRuntimeId;
    api.entityLevel = &entityLevel;
    api.entityPosition = &entityPosition;
    api.entityRotation = &entityRotation;
    api.entityTeleport = &entityTeleport;
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
    api.levelEntityCount = &levelEntityCount;
    api.levelEntity = &levelEntity;
}
