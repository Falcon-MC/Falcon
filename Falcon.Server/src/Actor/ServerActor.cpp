#include "Actor/ServerActor.h"

#include "Actor/DynamicPropertyStore.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/Movement/ActorMovementSystem.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Server/Profiler.h"
#include "Actor/ServerPlayer.h"
#include "Level/FalconDataVersion.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginManager.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <cmath>

namespace {
    const float SYNC_POSITION_EPSILON = 0.0001f;
    const int32_t INVULNERABILITY_TICKS = 10;
    const float ATTACK_KNOCKBACK = 0.4f;
    const float FIRE_TICK_DAMAGE = 1.0f;
    const int32_t SUNLIGHT_BURN_TICKS = 8 * 20;
    const int32_t DAYLIGHT_SUBTRACTED_THRESHOLD = 4;
}

namespace {
    Tag floatList3(float x, float y, float z) {
        Tag list = Tag::ofList(Tag::Type::Float);
        list.addToList(Tag::ofFloat(x));
        list.addToList(Tag::ofFloat(y));
        list.addToList(Tag::ofFloat(z));
        return list;
    }

    float listValue(const Tag &data, const char *key, size_t index, float fallback) {
        const Tag *list = data.get(key);
        if (list == nullptr || !list->isList())
            return fallback;

        const std::vector<Tag> &values = list->getList();
        if (index >= values.size())
            return fallback;

        return values[index].asFloat();
    }
}

ServerActor::ServerActor(uint64_t runtimeId, const std::string &identifier)
        : Actor(runtimeId), mIdentifier(identifier) {}

void ServerActor::tick(ServerNetworkHandler &owner) {
    owner.getProfiler().beginSection(ProfilerSection::ActorEnvironment);
    tickFire(owner);
    tickSunlightBurn(owner);
    owner.getProfiler().endSection(ProfilerSection::ActorEnvironment);

    owner.getProfiler().beginSection(ProfilerSection::ActorPhysics);
    ActorMovementSystem::tick(owner, *this);
    owner.getProfiler().endSection(ProfilerSection::ActorPhysics);
}

PhysicsComponent ServerActor::getPhysics() const {
    PhysicsComponent physics;
    physics.mHasGravity = hasGravity();
    physics.mPushable = isPushable();
    return physics;
}

bool ServerActor::needsMovementSync() const {
    if (!mMovementSynced)
        return true;

    const Vector3f position = getPosition();
    const Vector3f rotation = getRotation();
    return std::fabs(position.x - mSyncedPosition.x) > SYNC_POSITION_EPSILON
           || std::fabs(position.y - mSyncedPosition.y) > SYNC_POSITION_EPSILON
           || std::fabs(position.z - mSyncedPosition.z) > SYNC_POSITION_EPSILON
           || rotation.x != mSyncedRotation.x || rotation.y != mSyncedRotation.y || rotation.z != mSyncedRotation.z;
}

void ServerActor::markMovementSynced() {
    mSyncedPosition = getPosition();
    mSyncedRotation = getRotation();
    mMovementSynced = true;
}

void ServerActor::tickSunlightBurn(ServerNetworkHandler &owner) {
    if (!burnsInDaylight() || isOnFire() || hasEffect(MobEffectId::FireResistance))
        return;

    Level &level = owner.getLevelFor(*this);
    if (!level.hasSkyLight() || level.isRaining() || level.getSkyLightSubtracted() >= DAYLIGHT_SUBTRACTED_THRESHOLD)
        return;

    const Vector3f position = getPosition();
    const int32_t headY = (int32_t) std::floor(position.y) + 1;
    const int32_t blockX = (int32_t) std::floor(position.x);
    const int32_t blockZ = (int32_t) std::floor(position.z);

    if (level.getHeightAt(blockX, blockZ) > headY)
        return;

    if (LiquidBlocksFetch::at(level, position).water)
        return;

    setFireTicks(SUNLIGHT_BURN_TICKS);
    setOnFire(true);
    owner.syncActorFlags(*this);
}

void ServerActor::tickFire(ServerNetworkHandler &owner) {
    if (getFireTicks() <= 0)
        return;

    if (hasEffect(MobEffectId::FireResistance)) {
        setFireTicks(0);
        setOnFire(false);
        owner.syncActorFlags(*this);
        return;
    }

    if (getFireTicks() % 20 == 0)
        hurt(owner, FIRE_TICK_DAMAGE, nullptr);

    setFireTicks(getFireTicks() - 1);

    if (getFireTicks() <= 0) {
        setOnFire(false);
        owner.syncActorFlags(*this);
    }
}

bool ServerActor::hurt(ServerNetworkHandler &owner, float amount, Actor *attacker, int32_t lootingLevel) {
    if (!isAlive() || amount < 0.0f || isInvulnerable())
        return false;

    if (getNoDamageTicks() > 0 && amount <= getLastDamageAmount())
        return false;

    const char *cause = attacker != nullptr ? "entityAttack" : "none";
    if (owner.getScriptEngine().beforeEntityHurt(*this, amount, cause, attacker))
        return false;

    PluginEvent damageEvent;
    damageEvent.mType = FALCON_EVENT_ENTITY_DAMAGE;
    damageEvent.mCancellable = true;
    damageEvent.mEntity = this;
    damageEvent.mAttacker = attacker;
    damageEvent.mAmount = amount;
    damageEvent.mCause = attacker == nullptr ? "death.attack.generic"
                                             : attacker->isPlayer() ? "death.attack.player" : "death.attack.mob";
    PluginManager::getInstance().dispatch(damageEvent);
    if (damageEvent.mCancelled)
        return false;

    amount = (float) damageEvent.mAmount;
    if (amount <= 0.0f)
        return false;

    ServerPlayer *source = dynamic_cast<ServerPlayer *>(attacker);
    if (onHurt(owner, amount, source))
        return true;

    setHealth(getHealth() - amount);
    setNoDamageTicks(INVULNERABILITY_TICKS);
    setLastDamageAmount(amount);
    onDamaged(owner, attacker);

    if (attacker != nullptr && getHealth() > 0.0f && catchFireFrom(*attacker, owner.getProperties().getDifficulty()))
        owner.syncActorFlags(*this);

    if (getHealth() > 0.0f)
        owner.syncActorAttributes(*this);

    owner.broadcastActorEvent(*this, EntityEventType::HurtAnimation);
    owner.playLevelSound(owner.getLevelFor(*this), LevelSoundEvent::HIT, getPosition(), mIdentifier);

    if (source != nullptr) {
        const Vector3f sourcePosition = source->getPosition();
        owner.knockBack(*this, getPosition().x - sourcePosition.x, getPosition().z - sourcePosition.z,
                        ATTACK_KNOCKBACK);
    }

    if (getHealth() <= 0.0f) {
        int32_t looting = lootingLevel;
        if (looting < 0) {
            looting = source != nullptr
                      ? ItemEnchantments::getLevel(source->getInventory().getItemInHand(), EnchantmentIds::LOOTING)
                      : 0;
        }
        kill(owner, source, looting);
    }

    return true;
}

void ServerActor::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t lootingLevel) {
    (void) lootingLevel;

    if (isDead())
        return;

    setHealth(0.0f);
    owner.syncActorAttributes(*this);
    owner.broadcastActorEvent(*this, getDeathEvent());
    setDead(true);
    setMotion(Vector3f(0.0f, 0.0f, 0.0f));

    PluginEvent event;
    event.mType = FALCON_EVENT_ENTITY_DEATH;
    event.mEntity = this;
    event.mAttacker = source;
    PluginManager::getInstance().dispatch(event);
}

int32_t ServerActor::getIntProperty(const std::string &name, int32_t fallback) const {
    const auto it = mIntProperties.find(name);
    return it == mIntProperties.end() ? fallback : it->second;
}

float ServerActor::getFloatProperty(const std::string &name, float fallback) const {
    const auto it = mFloatProperties.find(name);
    return it == mFloatProperties.end() ? fallback : it->second;
}

Tag ServerActor::saveNbt() const {
    Tag data = Tag::ofCompound();

    data.putString("identifier", mIdentifier);
    data.putLong(FalconDataVersion::TAG, FalconDataVersion::CURRENT);
    data.put("Pos", floatList3(mPosition.x, mPosition.y, mPosition.z));
    data.put("Rotation", floatList3(mRotation.x, mRotation.y, mRotation.z));
    data.put("Motion", floatList3(mMotion.x, mMotion.y, mMotion.z));

    data.putFloat("Health", getHealth());
    data.putFloat("MaxHealth", getMaxHealth());
    data.putString("NameTag", mNameTag);
    data.putByte("Persistent", mPersistent ? 1 : 0);
    data.putLong("OwnerUniqueId", mOwnerUniqueId);
    saveTags(data);

    Tag intProperties = Tag::ofCompound();
    for (const auto &entry: mIntProperties)
        intProperties.putInt(entry.first, entry.second);
    data.put("IntProperties", intProperties);

    Tag floatProperties = Tag::ofCompound();
    for (const auto &entry: mFloatProperties)
        floatProperties.putFloat(entry.first, entry.second);
    data.put("FloatProperties", floatProperties);

    data.put("DynamicProperties", serializeDynamicProperties(mDynamicProperties));

    return data;
}

void ServerActor::loadNbt(const Tag &data) {
    if (!data.isCompound())
        return;

    mPosition = Vector3f(listValue(data, "Pos", 0, mPosition.x),
                         listValue(data, "Pos", 1, mPosition.y),
                         listValue(data, "Pos", 2, mPosition.z));

    mRotation = Vector3f(listValue(data, "Rotation", 0, 0.0f),
                         listValue(data, "Rotation", 1, 0.0f),
                         listValue(data, "Rotation", 2, 0.0f));

    mMotion = Vector3f(listValue(data, "Motion", 0, 0.0f),
                       listValue(data, "Motion", 1, 0.0f),
                       listValue(data, "Motion", 2, 0.0f));

    setMaxHealth(data.getFloat("MaxHealth", getMaxHealth()));
    setHealth(data.getFloat("Health", getHealth()));
    mNameTag = data.getString("NameTag", mNameTag);
    mPersistent = data.getByte("Persistent", 1) != 0;
    mOwnerUniqueId = data.getLong("OwnerUniqueId", mOwnerUniqueId);

    loadTags(data);

    const Tag *intProperties = data.get("IntProperties");
    if (intProperties != nullptr && intProperties->isCompound()) {
        for (const std::string &name: intProperties->getKeys())
            mIntProperties[name] = intProperties->getInt(name, 0);
    }

    const Tag *floatProperties = data.get("FloatProperties");
    if (floatProperties != nullptr && floatProperties->isCompound()) {
        for (const std::string &name: floatProperties->getKeys())
            mFloatProperties[name] = floatProperties->getFloat(name, 0.0f);
    }

    const Tag *dynamicProperties = data.get("DynamicProperties");
    if (dynamicProperties != nullptr)
        deserializeDynamicProperties(*dynamicProperties, mDynamicProperties);
}
