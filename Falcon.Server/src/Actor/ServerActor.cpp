#include "Actor/ServerActor.h"

#include "Actor/DynamicPropertyStore.h"
#include "Actor/ExperienceValues.h"
#include "Actor/MobLootTable.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Server/Profiler.h"
#include "Actor/ServerPlayer.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <cmath>

namespace {
    const float LIVING_GRAVITY = 0.08f;
    const float LIVING_DRAG = 0.98f;
    const float GROUND_FRICTION = 0.6f;
    const float AIR_FRICTION = 0.91f;
    const float MOTION_EPSILON = 0.003f;
    const int32_t INVULNERABILITY_TICKS = 10;
    const float ATTACK_KNOCKBACK = 0.4f;
    const float FIRE_TICK_DAMAGE = 1.0f;
    const int32_t SUNLIGHT_BURN_TICKS = 8 * 20;
    const int32_t DAYLIGHT_SUBTRACTED_THRESHOLD = 4;

    bool burnsInSunlight(const std::string &identifier) {
        static const std::unordered_set<std::string> burning = {
                "minecraft:zombie",
                "minecraft:zombie_villager",
                "minecraft:zombie_villager_v2",
                "minecraft:skeleton",
                "minecraft:stray",
                "minecraft:phantom",
                "minecraft:drowned"
        };

        return burning.count(identifier) != 0;
    }

    bool isFlyingActor(const std::string &identifier) {
        return identifier == "minecraft:allay" || identifier == "minecraft:bat" ||
               identifier == "minecraft:bee" || identifier == "minecraft:blaze" ||
               identifier == "minecraft:ender_dragon" || identifier == "minecraft:ghast" ||
               identifier == "minecraft:happy_ghast" || identifier == "minecraft:parrot" ||
               identifier == "minecraft:phantom" || identifier == "minecraft:shulker_bullet" ||
               identifier == "minecraft:vex" || identifier == "minecraft:wither";
    }
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
    _tickPhysics(owner);
    owner.getProfiler().endSection(ProfilerSection::ActorPhysics);
}

void ServerActor::_tickPhysics(ServerNetworkHandler &owner) {
    Vector3f motion = getMotion();
    if (std::fabs(motion.x) < MOTION_EPSILON && std::fabs(motion.y) < MOTION_EPSILON &&
        std::fabs(motion.z) < MOTION_EPSILON)
        return;

    Level &level = owner.getLevel();
    const Vector3f position = getPosition();
    const int32_t footX = (int32_t) std::floor(position.x);
    const int32_t footZ = (int32_t) std::floor(position.z);
    const bool onGround = level.isSolidAt(footX, (int32_t) std::floor(position.y - 0.1f), footZ);

    if (!onGround || motion.y > 0.0f) {
        motion.y -= LIVING_GRAVITY;
        motion.y *= LIVING_DRAG;
    }

    Vector3f target = position;
    target.x += motion.x;
    target.y += motion.y;
    target.z += motion.z;

    if (level.isSolidAt((int32_t) std::floor(target.x), (int32_t) std::floor(position.y), footZ)) {
        target.x = position.x;
        motion.x = 0.0f;
    }

    if (level.isSolidAt(footX, (int32_t) std::floor(position.y), (int32_t) std::floor(target.z))) {
        target.z = position.z;
        motion.z = 0.0f;
    }

    bool landed = false;
    if (motion.y < 0.0f && level.isSolidAt((int32_t) std::floor(target.x), (int32_t) std::floor(target.y),
                                           (int32_t) std::floor(target.z))) {
        target.y = std::floor(target.y) + 1.0f;
        motion.y = 0.0f;
        landed = true;
    }

    const float friction = onGround ? GROUND_FRICTION : AIR_FRICTION;
    motion.x *= friction;
    motion.z *= friction;

    setMotion(motion);
    setPosition(target);
    setOnGround(onGround || landed);

    float pendingFallDamage = 0.0f;
    if (!isFlyingActor(mIdentifier)) {
        if (target.y > getHighestPosition())
            setHighestPosition(target.y);

        updateFallDistance();

        if (landed || onGround) {
            pendingFallDamage = computeFallDamage();
            resetFallDistance();
        }
    }

    owner.broadcastActorMove(*this);

    if (pendingFallDamage > 0.0f)
        hurt(owner, pendingFallDamage, nullptr);
}

void ServerActor::tickSunlightBurn(ServerNetworkHandler &owner) {
    if (!burnsInSunlight(mIdentifier) || isOnFire() || hasEffect(MobEffectId::FireResistance))
        return;

    Level &level = owner.getLevel();
    if (level.isRaining() || level.getSkyLightSubtracted() >= DAYLIGHT_SUBTRACTED_THRESHOLD)
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

bool ServerActor::hurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source, int32_t lootingLevel) {
    if (!isAlive() || amount < 0.0f)
        return false;

    if (getNoDamageTicks() > 0 && amount <= getLastDamageAmount())
        return false;

    setHealth(getHealth() - amount);
    setNoDamageTicks(INVULNERABILITY_TICKS);
    setLastDamageAmount(amount);

    owner.broadcastActorEvent(*this, EntityEventType::HurtAnimation);
    owner.playLevelSound(LevelSoundEvent::HIT, getPosition(), mIdentifier);

    if (source != nullptr) {
        const Vector3f sourcePosition = source->getPosition();
        owner.knockBack(*this, getPosition().x - sourcePosition.x, getPosition().z - sourcePosition.z,
                        ATTACK_KNOCKBACK);
    }

    if (getHealth() <= 0.0f) {
        owner.broadcastActorEvent(*this, EntityEventType::DeathAnimation);

        const Vector3f dropPosition = getPosition();
        int32_t looting = lootingLevel;
        if (looting < 0) {
            looting = source != nullptr
                      ? ItemEnchantments::getLevel(source->getInventory().getItemInHand(), EnchantmentIds::LOOTING)
                      : 0;
        }
        for (const MobDrop &drop: MobLootTable::getMobDrops(mIdentifier, isOnFire(), looting))
            owner.spawnItemActor(drop.mItemIdentifier, drop.mCount, dropPosition);

        const int experience = ExperienceValues::getMobDropExperience(mIdentifier);
        if (experience > 0)
            owner.spawnExperienceOrbs(dropPosition, experience);

        setDead(true);
        setMotion(Vector3f(0.0f, 0.0f, 0.0f));
    }

    return true;
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
    data.put("Pos", floatList3(mPosition.x, mPosition.y, mPosition.z));
    data.put("Rotation", floatList3(mRotation.x, mRotation.y, mRotation.z));
    data.put("Motion", floatList3(mMotion.x, mMotion.y, mMotion.z));

    data.putFloat("Health", getHealth());
    data.putFloat("MaxHealth", getMaxHealth());
    data.putString("NameTag", mNameTag);
    data.putLong("OwnerUniqueId", mOwnerUniqueId);

    Tag tags = Tag::ofList(Tag::Type::String);
    for (const std::string &tag: mTags)
        tags.addToList(Tag::ofString(tag));
    data.put("Tags", tags);

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
    mOwnerUniqueId = data.getLong("OwnerUniqueId", mOwnerUniqueId);

    const Tag *tags = data.get("Tags");
    if (tags != nullptr && tags->isList()) {
        for (const Tag &entry: tags->getList())
            mTags.insert(entry.asString());
    }

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
