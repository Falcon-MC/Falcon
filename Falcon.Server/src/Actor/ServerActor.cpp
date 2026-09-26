#include "Actor/ServerActor.h"

#include "Actor/Definition/EntityDefinitions.h"
#include "Actor/Definition/Molang.h"
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
#include "Scripting/Content/CustomContentRegistry.h"

#include <algorithm>
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

    const char *const HEALTH_ATTRIBUTE = "minecraft:health";

    Tag attributesTag(const ActorAttributes &attributes) {
        std::vector<Tag> list;
        for (const AttributeData &attribute: attributes.getAll()) {
            Tag entry = Tag::ofCompound();
            entry.putString("Name", attribute.mName);
            entry.putFloat("Base", attribute.mName == HEALTH_ATTRIBUTE ? attribute.mMaximum : attribute.mValue);
            entry.putFloat("Current", attribute.mValue);
            entry.putFloat("Min", attribute.mMinimum);
            entry.putFloat("Max", attribute.mMaximum);
            entry.putFloat("DefaultMin", attribute.mDefaultMinimum);
            entry.putFloat("DefaultMax", attribute.mDefaultMaximum);
            list.push_back(std::move(entry));
        }
        return Tag::ofList(Tag::Type::Compound, std::move(list));
    }

    Tag propertiesTag(const ServerActor &actor) {
        Tag properties = Tag::ofCompound();
        const std::vector<ActorPropertyDescription> *schema = actor.getPropertySchema();
        if (schema == nullptr)
            return properties;

        for (const ActorPropertyDescription &descriptor: *schema) {
            switch (descriptor.mType) {
                case ActorPropertyDescription::Type::Int:
                    properties.putInt(descriptor.mName, actor.getIntProperty(descriptor.mName, descriptor.mDefaultInt));
                    break;
                case ActorPropertyDescription::Type::Float:
                    properties.putFloat(descriptor.mName,
                                        actor.getFloatProperty(descriptor.mName, descriptor.mDefaultFloat));
                    break;
                case ActorPropertyDescription::Type::Bool:
                    properties.putByte(descriptor.mName,
                                       actor.getIntProperty(descriptor.mName, descriptor.mDefaultBool ? 1 : 0) != 0
                                       ? 1 : 0);
                    break;
                case ActorPropertyDescription::Type::Enum: {
                    const int32_t index = actor.getIntProperty(descriptor.mName, descriptor.mDefaultInt);
                    if (index >= 0 && index < (int32_t) descriptor.mEnumValues.size())
                        properties.putString(descriptor.mName, descriptor.mEnumValues[(size_t) index]);
                    break;
                }
            }
        }
        return properties;
    }

    void loadProperties(ServerActor &actor, const Tag &properties) {
        const std::vector<ActorPropertyDescription> *schema = actor.getPropertySchema();
        if (schema == nullptr)
            return;

        for (const ActorPropertyDescription &descriptor: *schema) {
            const Tag *value = properties.get(descriptor.mName);
            if (value == nullptr)
                continue;

            switch (descriptor.mType) {
                case ActorPropertyDescription::Type::Int:
                    actor.setIntProperty(descriptor.mName, properties.getInt(descriptor.mName, descriptor.mDefaultInt));
                    break;
                case ActorPropertyDescription::Type::Float:
                    actor.setFloatProperty(descriptor.mName,
                                           properties.getFloat(descriptor.mName, descriptor.mDefaultFloat));
                    break;
                case ActorPropertyDescription::Type::Bool:
                    actor.setIntProperty(descriptor.mName, properties.getByte(descriptor.mName, 0) != 0 ? 1 : 0);
                    break;
                case ActorPropertyDescription::Type::Enum: {
                    const int32_t index = descriptor.findEnumIndex(properties.getString(descriptor.mName, std::string()));
                    if (index >= 0)
                        actor.setIntProperty(descriptor.mName, index);
                    break;
                }
            }
        }
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
        hurt(owner, FIRE_TICK_DAMAGE, ActorDamageSource::environment("death.attack.inFire", getName()));

    setFireTicks(getFireTicks() - 1);

    if (getFireTicks() <= 0) {
        setOnFire(false);
        owner.syncActorFlags(*this);
    }
}

bool ServerActor::hurt(ServerNetworkHandler &owner, float amount, Actor *attacker, int32_t lootingLevel) {
    ActorDamageSource source;
    source.mDeathMessageKey = attacker == nullptr ? "death.attack.generic"
                                                  : attacker->isPlayer() ? "death.attack.player" : "death.attack.mob";
    source.mAttacker = attacker;
    return hurt(owner, amount, source, lootingLevel);
}

bool ServerActor::hurt(ServerNetworkHandler &owner, float amount, const ActorDamageSource &damageSource,
                       int32_t lootingLevel) {
    Actor *attacker = damageSource.mAttacker;
    if (!isAlive() || amount < 0.0f || isInvulnerable())
        return false;

    if (getNoDamageTicks() > 0 && amount <= getLastDamageAmount())
        return false;

    if (!senseDamage(owner, amount, damageSource))
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
    damageEvent.mCause = damageSource.mDeathMessageKey;
    PluginManager::getInstance().dispatch(damageEvent);
    if (damageEvent.mCancelled)
        return false;

    amount = (float) damageEvent.mAmount;
    if (amount <= 0.0f)
        return false;

    ServerPlayer *source = dynamic_cast<ServerPlayer *>(attacker);
    if (source != nullptr)
        source->recordAttacked(getRuntimeId(), owner.getCurrentTick());

    const float rawAmount = amount;
    if (damageSource.mApplyArmor)
        amount = absorbDamage(amount, damageSource);

    if (onHurt(owner, amount, source))
        return true;

    setHealth(getHealth() - amount);
    setNoDamageTicks(INVULNERABILITY_TICKS);
    setLastDamageAmount(rawAmount);
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

const std::vector<ActorPropertyDescription> *ServerActor::getPropertySchema() const {
    if (mDefinition != nullptr)
        return &mDefinition->mProperties;
    return EntityDefinitions::findProperties(mIdentifier);
}

const ActorPropertyDescription *ServerActor::findPropertyDescription(const std::string &name) const {
    const std::vector<ActorPropertyDescription> *schema = getPropertySchema();
    return schema == nullptr ? nullptr : ActorPropertySchema::find(*schema, name);
}

void ServerActor::initializeProperties() {
    const std::vector<ActorPropertyDescription> *schema = getPropertySchema();
    if (schema == nullptr)
        return;

    for (const ActorPropertyDescription &descriptor: *schema) {
        const bool computed = !descriptor.mDefaultExpression.empty();
        if (descriptor.mType == ActorPropertyDescription::Type::Float) {
            if (!hasFloatProperty(descriptor.mName))
                setFloatProperty(descriptor.mName, computed
                                                   ? (float) Molang::evaluate(descriptor.mDefaultExpression,
                                                                              *this).mNumber
                                                   : descriptor.mDefaultFloat);
            continue;
        }

        if (hasIntProperty(descriptor.mName))
            continue;

        int32_t value = descriptor.mDefaultInt;
        if (computed) {
            const MolangValue result = Molang::evaluate(descriptor.mDefaultExpression, *this);
            value = descriptor.mType == ActorPropertyDescription::Type::Bool ? (result.isTrue() ? 1 : 0)
                                                                             : (int32_t) std::lround(result.mNumber);
        }
        setIntProperty(descriptor.mName, value);
    }
}

bool ServerActor::assignProperty(const ActorPropertyDescription &descriptor, const json::Value &value) {
    const std::string &name = descriptor.mName;
    const bool expression = value.isString();

    if (descriptor.mType == ActorPropertyDescription::Type::Float) {
        float result = expression ? (float) Molang::evaluate(value.mString, *this).mNumber : (float) value.number();
        if (descriptor.mMaxFloat > descriptor.mMinFloat)
            result = std::min(std::max(result, descriptor.mMinFloat), descriptor.mMaxFloat);

        const bool changed = !hasFloatProperty(name) || getFloatProperty(name) != result;
        setFloatProperty(name, result);
        return changed;
    }

    int32_t result = 0;
    if (descriptor.mType == ActorPropertyDescription::Type::Enum) {
        result = descriptor.findEnumIndex(value.string());
        if (result < 0 && expression) {
            const MolangValue evaluated = Molang::evaluate(value.mString, *this);
            result = evaluated.mIsString ? descriptor.findEnumIndex(evaluated.mString)
                                         : (int32_t) std::lround(evaluated.mNumber);
        }
        if (result < 0 || result >= (int32_t) descriptor.mEnumValues.size())
            return false;
    } else if (descriptor.mType == ActorPropertyDescription::Type::Bool) {
        if (value.mType == json::Value::Type::Boolean)
            result = value.mBoolean ? 1 : 0;
        else
            result = (expression ? Molang::evaluate(value.mString, *this).isTrue() : value.number() != 0.0) ? 1 : 0;
    } else {
        const double number = expression ? Molang::evaluate(value.mString, *this).mNumber : value.number();
        result = (int32_t) std::lround(number);
        if (descriptor.mMaxInt > descriptor.mMinInt)
            result = std::min(std::max(result, descriptor.mMinInt), descriptor.mMaxInt);
    }

    const bool changed = !hasIntProperty(name) || getIntProperty(name) != result;
    setIntProperty(name, result);
    return changed;
}

Tag ServerActor::saveNbt() const {
    Tag data = Tag::ofCompound();

    data.putString("identifier", mIdentifier);
    data.putLong("UniqueID", getUniqueId());
    data.putLong(FalconDataVersion::TAG, FalconDataVersion::CURRENT);
    data.put("Pos", floatList3(mPosition.x, mPosition.y, mPosition.z));
    Tag rotation = Tag::ofList(Tag::Type::Float);
    rotation.addToList(Tag::ofFloat(mRotation.y));
    rotation.addToList(Tag::ofFloat(mRotation.x));
    data.put("Rotation", rotation);
    data.put("Motion", floatList3(mMotion.x, mMotion.y, mMotion.z));

    data.put("Attributes", attributesTag(getAttributes()));
    if (!mNameTag.empty())
        data.putString("CustomName", mNameTag);
    data.putByte("Persistent", mPersistent ? 1 : 0);
    data.putLong("OwnerUniqueId", mOwnerUniqueId);
    saveTags(data);

    if (hasPassengers()) {
        std::vector<Tag> links;
        const std::vector<int64_t> &passengers = getPassengers();
        for (size_t index = 0; index < passengers.size(); ++index) {
            Tag link = Tag::ofCompound();
            link.putLong("entityID", passengers[index]);
            link.putInt("linkID", (int32_t) index);
            links.push_back(std::move(link));
        }
        data.put("LinksTag", Tag::ofList(Tag::Type::Compound, std::move(links)));
    }

    const Tag properties = propertiesTag(*this);
    if (!properties.getKeys().empty())
        data.put("properties", properties);

    data.put("DynamicProperties", serializeDynamicProperties(mDynamicProperties));

    return data;
}

void ServerActor::loadNbt(const Tag &data) {
    if (!data.isCompound())
        return;

    if (data.contains("UniqueID"))
        setUniqueId(data.getLong("UniqueID"));

    mPosition = Vector3f(listValue(data, "Pos", 0, mPosition.x),
                         listValue(data, "Pos", 1, mPosition.y),
                         listValue(data, "Pos", 2, mPosition.z));

    if (data.contains(FalconDataVersion::TAG) && !data.contains("Attributes")) {
        mRotation = Vector3f(listValue(data, "Rotation", 0, 0.0f),
                             listValue(data, "Rotation", 1, 0.0f),
                             listValue(data, "Rotation", 2, 0.0f));
    } else {
        const float yaw = listValue(data, "Rotation", 0, 0.0f);
        mRotation = Vector3f(listValue(data, "Rotation", 1, 0.0f), yaw, yaw);
    }

    mMotion = Vector3f(listValue(data, "Motion", 0, 0.0f),
                       listValue(data, "Motion", 1, 0.0f),
                       listValue(data, "Motion", 2, 0.0f));

    const Tag *attributes = data.get("Attributes");
    if (attributes != nullptr && attributes->isList()) {
        for (const Tag &attribute: attributes->getList()) {
            if (!attribute.isCompound())
                continue;

            const std::string name = attribute.getString("Name", std::string());
            if (name == HEALTH_ATTRIBUTE) {
                setMaxHealth(attribute.getFloat("Max", getMaxHealth()));
                setHealth(attribute.getFloat("Current", getHealth()));
            } else if (!name.empty()) {
                getAttributes().setClamped(name, attribute.getFloat("Current", getAttributes().get(name)));
            }
        }
    } else {
        setMaxHealth(data.getFloat("MaxHealth", getMaxHealth()));
        setHealth(data.getFloat("Health", getHealth()));
    }

    mNameTag = data.getString("CustomName", data.getString("NameTag", mNameTag));
    mPersistent = data.getByte("Persistent", 1) != 0;
    mOwnerUniqueId = data.getLong("OwnerUniqueId", mOwnerUniqueId);

    loadTags(data);

    mPendingPassengers.clear();
    const Tag *links = data.get("LinksTag");
    if (links != nullptr && links->isList()) {
        for (const Tag &link: links->getList()) {
            if (link.isCompound() && link.contains("entityID"))
                mPendingPassengers.push_back(link.getLong("entityID"));
        }
    }

    const Tag *properties = data.get("properties");
    if (properties != nullptr && properties->isCompound())
        loadProperties(*this, *properties);

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
