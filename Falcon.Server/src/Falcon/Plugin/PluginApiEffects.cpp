#include "Falcon/Plugin/PluginServerApi.h"

#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/StopSoundPacket.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace PluginApiHelpers;

namespace {
    constexpr int32_t MAX_AMPLIFIER = 255;

    Vector3f toVector3f(const FalconVec3 &value) {
        return Vector3f((float) value.x, (float) value.y, (float) value.z);
    }

    std::vector<int32_t> sortedEffects(const Actor &actor) {
        std::vector<int32_t> ids;
        for (const auto &entry: actor.getEffects().getAll())
            ids.push_back(entry.first);
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    void writeEffect(const MobEffectInstance &instance, FalconEffect *out) {
        out->type = (FalconEffectType) instance.mId;
        out->duration = instance.mInfinite ? FALCON_EFFECT_INFINITE : instance.mDuration;
        out->amplifier = instance.mAmplifier;
        out->showParticles = instance.mParticles ? 1 : 0;
        out->ambient = instance.mAmbient ? 1 : 0;
    }

    // players send their effect changes themselves; other entities need the packet
    void syncEffect(Actor &actor, MobEffectId id) {
        ServerActor *serverActor = dynamic_cast<ServerActor *>(&actor);
        if (serverActor == nullptr)
            return;

        const MobEffectInstance *applied = actor.getEffect(id);
        if (applied == nullptr)
            owner().applyActorEffect(*serverActor, (int32_t) id, 0, 0, false);
        else
            owner().applyActorEffect(*serverActor, (int32_t) id, applied->mAmplifier, applied->mDuration,
                                     applied->mParticles);
    }

    int entityAddEffect(FalconEntity *target, FalconEffectType type, int32_t duration, int32_t amplifier,
                        int showParticles) {
        if (!isKnownMobEffect((int32_t) type) || amplifier < 0 || amplifier > MAX_AMPLIFIER)
            return 0;
        if (duration <= 0 && duration != FALCON_EFFECT_INFINITE)
            return 0;

        Actor *value = entity(target);
        MobEffectInstance instance;
        instance.mId = (MobEffectId) type;
        instance.mInfinite = duration == FALCON_EFFECT_INFINITE;
        instance.mDuration = duration;
        instance.mAmplifier = amplifier;
        instance.mParticles = showParticles != 0;
        if (!value->addEffect(instance))
            return 0;

        syncEffect(*value, instance.mId);
        return 1;
    }

    int entityRemoveEffect(FalconEntity *target, FalconEffectType type) {
        if (!isKnownMobEffect((int32_t) type))
            return 0;

        Actor *value = entity(target);
        if (!value->removeEffect((MobEffectId) type))
            return 0;

        syncEffect(*value, (MobEffectId) type);
        return 1;
    }

    int entityEffect(FalconEntity *target, FalconEffectType type, FalconEffect *out) {
        if (out == nullptr || !isKnownMobEffect((int32_t) type))
            return 0;

        const MobEffectInstance *instance = entity(target)->getEffect((MobEffectId) type);
        if (instance == nullptr)
            return 0;

        writeEffect(*instance, out);
        return 1;
    }

    uint32_t entityEffectCount(FalconEntity *target) {
        return (uint32_t) entity(target)->getEffects().getAll().size();
    }

    int entityEffectAt(FalconEntity *target, uint32_t index, FalconEffect *out) {
        if (out == nullptr)
            return 0;

        const Actor *value = entity(target);
        const std::vector<int32_t> ids = sortedEffects(*value);
        if (index >= ids.size())
            return 0;

        writeEffect(*value->getEffect((MobEffectId) ids[index]), out);
        return 1;
    }

    void entityClearEffects(FalconEntity *target) {
        Actor *value = entity(target);
        for (const int32_t id: sortedEffects(*value)) {
            if (value->removeEffect((MobEffectId) id))
                syncEffect(*value, (MobEffectId) id);
        }
    }

    const AttributeData *findAttribute(const Actor &actor, const char *name) {
        if (name == nullptr)
            return nullptr;

        for (const AttributeData &attribute: actor.getAttributes().getAll()) {
            if (attribute.mName == name)
                return &attribute;
        }
        return nullptr;
    }

    uint32_t entityAttributeCount(FalconEntity *target) {
        return (uint32_t) entity(target)->getAttributes().getAll().size();
    }

    const char *entityAttributeName(FalconEntity *target, uint32_t index) {
        const std::vector<AttributeData> &attributes = entity(target)->getAttributes().getAll();
        return index < attributes.size() ? hold(attributes[index].mName) : nullptr;
    }

    int entityAttribute(FalconEntity *target, const char *name, FalconAttribute *out) {
        const AttributeData *attribute = findAttribute(*entity(target), name);
        if (attribute == nullptr || out == nullptr)
            return 0;

        out->value = attribute->mValue;
        out->minimum = attribute->mMinimum;
        out->maximum = attribute->mMaximum;
        out->defaultValue = attribute->mDefaultValue;
        return 1;
    }

    int entitySetAttribute(FalconEntity *target, const char *name, float value) {
        Actor *actor = entity(target);
        const AttributeData *attribute = findAttribute(*actor, name);
        // written so a nan fails it too
        if (attribute == nullptr || !(value >= attribute->mMinimum && value <= attribute->mMaximum))
            return 0;

        actor->getAttributes().set(attribute->mName, value);
        if (ServerPlayer *playerValue = dynamic_cast<ServerPlayer *>(actor))
            owner().syncPlayerAttributes(*playerValue);
        else if (ServerActor *serverActor = dynamic_cast<ServerActor *>(actor))
            owner().syncActorAttributes(*serverActor);
        return 1;
    }

    void playerPlaySound(FalconPlayer *target, const char *sound, FalconVec3 position, float volume, float pitch) {
        if (sound == nullptr || *sound == '\0')
            return;
        owner().playSoundFor(*player(target), sound, toVector3f(position), volume, pitch);
    }

    void levelPlaySound(FalconLevel *handle, const char *sound, FalconVec3 position, float volume, float pitch) {
        if (sound == nullptr || *sound == '\0')
            return;

        const Level &target = *level(handle);
        const Vector3f at = toVector3f(position);
        for (auto &entry: owner().getPlayers()) {
            if (entry.second.isSpawned() && entry.second.isIn(target))
                owner().playSoundFor(entry.second, sound, at, volume, pitch);
        }
    }

    void playerStopSound(FalconPlayer *target, const char *sound) {
        StopSoundPacket packet;
        packet.mStoppingAllSound = sound == nullptr || *sound == '\0';
        packet.mSoundName = packet.mStoppingAllSound ? std::string() : std::string(sound);
        packet.mStopMusicLegacy = false;
        owner().sendPacketTo(player(target)->getNetworkIdentifier(), packet);
    }

    void playerSpawnParticle(FalconPlayer *target, const char *particle, FalconVec3 position) {
        if (particle == nullptr || *particle == '\0')
            return;

        ServerPlayer *viewer = player(target);
        owner().spawnParticleEffect(owner().getLevelFor(*viewer), particle, toVector3f(position), nullptr, viewer);
    }

    void levelSpawnParticle(FalconLevel *handle, const char *particle, FalconVec3 position) {
        if (particle == nullptr || *particle == '\0')
            return;
        owner().spawnParticleEffect(*level(handle), particle, toVector3f(position));
    }
}

void PluginServerApi::fillEffects(FalconServerApi &api) {
    api.entityAddEffect = &entityAddEffect;
    api.entityRemoveEffect = &entityRemoveEffect;
    api.entityEffect = &entityEffect;
    api.entityEffectCount = &entityEffectCount;
    api.entityEffectAt = &entityEffectAt;
    api.entityClearEffects = &entityClearEffects;
    api.entityAttributeCount = &entityAttributeCount;
    api.entityAttributeName = &entityAttributeName;
    api.entityAttribute = &entityAttribute;
    api.entitySetAttribute = &entitySetAttribute;
    api.playerPlaySound = &playerPlaySound;
    api.levelPlaySound = &levelPlaySound;
    api.playerStopSound = &playerStopSound;
    api.playerSpawnParticle = &playerSpawnParticle;
    api.levelSpawnParticle = &levelSpawnParticle;
}
