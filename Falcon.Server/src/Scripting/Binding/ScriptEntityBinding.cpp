#include "Scripting/Binding/ScriptApi.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ActorDamageSource.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace ScriptValues;

namespace {
    struct EntityTarget {
        ScriptApi *mApi = nullptr;
        const ScriptEntityRef *mRef = nullptr;
        Actor *mActor = nullptr;
        ServerPlayer *mPlayer = nullptr;
        ServerActor *mServerActor = nullptr;
    };

    // the trampoline already refused a removed entity
    EntityTarget entityOf(JSContext *ctx, JSValueConst thisVal) {
        EntityTarget target;
        target.mApi = ScriptApi::fromContext(ctx);
        target.mRef = target.mApi->native<ScriptEntityRef>(thisVal);
        if (target.mRef == nullptr)
            return target;
        target.mActor = target.mApi->resolveEntity(*target.mRef);
        target.mPlayer = dynamic_cast<ServerPlayer *>(target.mActor);
        target.mServerActor = dynamic_cast<ServerActor *>(target.mActor);
        return target;
    }

    std::unordered_map<std::string, DynamicPropertyValue> *dynamicPropertiesOf(const EntityTarget &target) {
        if (target.mPlayer != nullptr)
            return &target.mPlayer->getDynamicProperties();
        if (target.mServerActor != nullptr)
            return &target.mServerActor->getDynamicProperties();
        return nullptr;
    }

    std::vector<std::string> familiesOf(const EntityTarget &target) {
        if (target.mServerActor != nullptr && target.mServerActor->getDefinition() != nullptr)
            return target.mServerActor->getDefinition()->mTypeFamilies;
        return std::vector<std::string>();
    }

    bool matchesQuery(JSContext *ctx, const EntityTarget &target, JSValueConst options) {
        if (!JS_IsObject(options))
            return true;

        const std::string typeId = target.mRef->mTypeId;
        JSValue typeValue = JS_GetPropertyStr(ctx, options, "type");
        const bool typeMismatch = JS_IsString(typeValue) && !toString(ctx, typeValue).empty()
                                  && toString(ctx, typeValue) != typeId;
        JS_FreeValue(ctx, typeValue);
        if (typeMismatch)
            return false;

        JSValue excludeTypes = JS_GetPropertyStr(ctx, options, "excludeTypes");
        const bool typeExcluded = arrayContainsString(ctx, excludeTypes, typeId);
        JS_FreeValue(ctx, excludeTypes);
        if (typeExcluded)
            return false;

        const std::vector<std::string> families = familiesOf(target);
        JSValue excludeFamilies = JS_GetPropertyStr(ctx, options, "excludeFamilies");
        bool familyExcluded = false;
        for (const std::string &family: families)
            familyExcluded = familyExcluded || arrayContainsString(ctx, excludeFamilies, family);
        JS_FreeValue(ctx, excludeFamilies);
        if (familyExcluded)
            return false;

        JSValue excludeTags = JS_GetPropertyStr(ctx, options, "excludeTags");
        bool tagExcluded = false;
        for (const std::string &tag: target.mActor->getTags())
            tagExcluded = tagExcluded || arrayContainsString(ctx, excludeTags, tag);
        JS_FreeValue(ctx, excludeTags);
        if (tagExcluded)
            return false;

        for (const char *key: {"families", "tags"}) {
            JSValue wanted = JS_GetPropertyStr(ctx, options, key);
            bool missing = false;
            if (JS_IsArray(wanted)) {
                uint32_t length = 0;
                JSValue lengthValue = JS_GetPropertyStr(ctx, wanted, "length");
                JS_ToUint32(ctx, &length, lengthValue);
                JS_FreeValue(ctx, lengthValue);
                for (uint32_t i = 0; i < length && !missing; ++i) {
                    JSValue element = JS_GetPropertyUint32(ctx, wanted, i);
                    const std::string value = toString(ctx, element);
                    JS_FreeValue(ctx, element);
                    if (std::strcmp(key, "tags") == 0)
                        missing = !target.mActor->hasTag(value);
                    else
                        missing = std::find(families.begin(), families.end(), value) == families.end();
                }
            }
            JS_FreeValue(ctx, wanted);
            if (missing)
                return false;
        }
        return true;
    }

    JSValue entityGetTypeId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        return string(ctx, target.mRef->mTypeId);
    }

    JSValue entityGetId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        return string(ctx, target.mRef->mEntityId);
    }

    JSValue entityGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, entityOf(ctx, thisVal).mActor != nullptr);
    }

    JSValue entityGetLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const Vector3f &position = entityOf(ctx, thisVal).mActor->getPosition();
        return vector3(ctx, position.x, position.y, position.z);
    }

    JSValue entityGetDimension(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        return target.mApi->makeDimension(target.mApi->host().getLevelFor(*target.mActor));
    }

    JSValue entityGetFlag(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const ActorFlag flag = magic == 0 ? ActorFlag::Sneaking : ActorFlag::Sprinting;
        return JS_NewBool(ctx, entityOf(ctx, thisVal).mActor->getFlags().get(flag));
    }

    JSValue entityGetNameTag(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (target.mServerActor != nullptr)
            return string(ctx, target.mServerActor->getNameTag());
        if (target.mPlayer != nullptr)
            return string(ctx, target.mPlayer->getName());
        return JS_NewString(ctx, "");
    }

    JSValue entitySetNameTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (target.mServerActor != nullptr && argc >= 1) {
            target.mServerActor->setNameTag(toString(ctx, argv[0]));
            target.mApi->host().sendActorNameTag(*target.mServerActor);
        }
        return JS_UNDEFINED;
    }

    void removeActorLater(ServerNetworkHandler &host, int64_t uniqueId) {
        host.postToMainThread([&host, uniqueId] {
            host.removeActor(uniqueId);
        });
    }

    JSValue entityRemove(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (target.mServerActor != nullptr)
            removeActorLater(target.mApi->host(), target.mServerActor->getUniqueId());
        ItemActor *item = dynamic_cast<ItemActor *>(target.mActor);
        if (item != nullptr)
            item->setRemoved(true);
        return JS_UNDEFINED;
    }

    JSValue entityTeleport(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        const Vector3f position((float) x, (float) y, (float) z);
        if (target.mPlayer != nullptr) {
            target.mPlayer->teleport(target.mApi->host(), position);
            return JS_UNDEFINED;
        }

        target.mActor->setPosition(position);
        if (argc >= 2 && JS_IsObject(argv[1])) {
            JSValue rotationValue = JS_GetPropertyStr(ctx, argv[1], "rotation");
            if (JS_IsObject(rotationValue)) {
                JSValue rx = JS_GetPropertyStr(ctx, rotationValue, "x");
                JSValue ry = JS_GetPropertyStr(ctx, rotationValue, "y");
                double pitch = 0.0, yaw = 0.0;
                JS_ToFloat64(ctx, &pitch, rx);
                JS_ToFloat64(ctx, &yaw, ry);
                target.mActor->setRotation(Vector3f((float) pitch, (float) yaw, 0.0f));
                JS_FreeValue(ctx, rx);
                JS_FreeValue(ctx, ry);
            }
            JS_FreeValue(ctx, rotationValue);
        }
        if (target.mServerActor != nullptr)
            target.mApi->host().broadcastActorMove(*target.mServerActor);
        return JS_UNDEFINED;
    }

    JSValue entityGetRotation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const Vector3f &rotation = entityOf(ctx, thisVal).mActor->getRotation();
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "x", JS_NewFloat64(ctx, rotation.x));
        JS_SetPropertyStr(ctx, object, "y", JS_NewFloat64(ctx, rotation.y));
        return object;
    }

    JSValue entitySetRotation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1 || !JS_IsObject(argv[0]) || target.mServerActor == nullptr)
            return JS_UNDEFINED;

        JSValue xValue = JS_GetPropertyStr(ctx, argv[0], "x");
        JSValue yValue = JS_GetPropertyStr(ctx, argv[0], "y");
        double pitch = 0.0, yaw = 0.0;
        JS_ToFloat64(ctx, &pitch, xValue);
        JS_ToFloat64(ctx, &yaw, yValue);
        JS_FreeValue(ctx, xValue);
        JS_FreeValue(ctx, yValue);

        target.mServerActor->setRotation(Vector3f((float) pitch, (float) yaw, 0.0f));
        target.mApi->host().broadcastActorMove(*target.mServerActor);
        return JS_UNDEFINED;
    }

    JSValue entityGetVelocity(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const Vector3f &motion = entityOf(ctx, thisVal).mActor->getMotion();
        return vector3(ctx, motion.x, motion.y, motion.z);
    }

    void setMotion(const EntityTarget &target, const Vector3f &motion) {
        target.mActor->setMotion(motion);
        target.mApi->host().sendActorMotion(*target.mActor);
    }

    JSValue entityClearVelocity(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        setMotion(entityOf(ctx, thisVal), Vector3f(0.0f, 0.0f, 0.0f));
        return JS_UNDEFINED;
    }

    JSValue entityApplyImpulse(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        const Vector3f &motion = target.mActor->getMotion();
        setMotion(target, Vector3f(motion.x + (float) x, motion.y + (float) y, motion.z + (float) z));
        return JS_UNDEFINED;
    }

    JSValue entityApplyKnockback(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        double dx = 0.0, dz = 0.0, vertical = 0.0;
        // 1.x takes a direction and two strengths, 2.0 a horizontal force and a vertical strength
        if (target.mApi->isStable1()) {
            double strength = 0.0;
            if (argc < 4)
                return JS_ThrowTypeError(ctx, "Incorrect number of arguments to function. Expected 4, received %d",
                                         argc);
            JS_ToFloat64(ctx, &dx, argv[0]);
            JS_ToFloat64(ctx, &dz, argv[1]);
            JS_ToFloat64(ctx, &strength, argv[2]);
            JS_ToFloat64(ctx, &vertical, argv[3]);
            dx *= strength;
            dz *= strength;
        } else {
            if (argc < 2 || !JS_IsObject(argv[0]))
                return JS_ThrowTypeError(ctx, "Native type conversion failed.");
            JSValue xValue = JS_GetPropertyStr(ctx, argv[0], "x");
            JSValue zValue = JS_GetPropertyStr(ctx, argv[0], "z");
            JS_ToFloat64(ctx, &dx, xValue);
            JS_ToFloat64(ctx, &dz, zValue);
            JS_FreeValue(ctx, xValue);
            JS_FreeValue(ctx, zValue);
            JS_ToFloat64(ctx, &vertical, argv[1]);
        }

        const Vector3f &motion = target.mActor->getMotion();
        setMotion(target, Vector3f(motion.x + (float) dx, (float) vertical, motion.z + (float) dz));
        return JS_UNDEFINED;
    }

    JSValue entityApplyDamage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        double amount = 0.0;
        if (argc >= 1)
            JS_ToFloat64(ctx, &amount, argv[0]);

        if (target.mPlayer != nullptr) {
            const ActorDamageSource source = ActorDamageSource::environment("death.attack.generic",
                                                                            target.mPlayer->getName());
            target.mApi->host().hurt(*target.mPlayer, (float) amount, source);
            return JS_TRUE;
        }

        target.mActor->setHealth(target.mActor->getHealth() - (float) amount);
        if (target.mServerActor != nullptr && target.mActor->getHealth() <= 0.0f)
            removeActorLater(target.mApi->host(), target.mServerActor->getUniqueId());
        return JS_TRUE;
    }

    JSValue entityKill(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (target.mPlayer != nullptr)
            target.mApi->host().killPlayer(*target.mPlayer, "death.attack.generic", {target.mPlayer->getName()});
        else if (target.mServerActor != nullptr)
            removeActorLater(target.mApi->host(), target.mServerActor->getUniqueId());
        return JS_TRUE;
    }

    JSValue entityAddEffect(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 2)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        MobEffectId id;
        if (!parseMobEffect(toString(ctx, argv[0]), id))
            return JS_UNDEFINED;

        int32_t duration = 0;
        JS_ToInt32(ctx, &duration, argv[1]);

        int32_t amplifier = 0;
        bool showParticles = true;
        if (argc >= 3 && JS_IsObject(argv[2])) {
            JSValue amplifierValue = JS_GetPropertyStr(ctx, argv[2], "amplifier");
            if (!JS_IsUndefined(amplifierValue))
                JS_ToInt32(ctx, &amplifier, amplifierValue);
            JS_FreeValue(ctx, amplifierValue);

            JSValue particlesValue = JS_GetPropertyStr(ctx, argv[2], "showParticles");
            if (!JS_IsUndefined(particlesValue))
                showParticles = JS_ToBool(ctx, particlesValue) == 1;
            JS_FreeValue(ctx, particlesValue);
        }

        if (target.mServerActor == nullptr) {
            target.mApi->host().addEffect(*target.mActor, id, amplifier, duration);
            return JS_UNDEFINED;
        }

        MobEffectInstance instance;
        instance.mId = id;
        instance.mDuration = duration;
        instance.mAmplifier = amplifier;
        target.mServerActor->addEffect(instance);
        target.mApi->host().applyActorEffect(*target.mServerActor, (int32_t) id, amplifier, duration, showParticles);
        return JS_UNDEFINED;
    }

    JSValue entityGetEffect(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        MobEffectId id;
        if (argc < 1 || !parseMobEffect(toString(ctx, argv[0]), id))
            return JS_UNDEFINED;

        const MobEffectInstance *effect = target.mActor->getEffect(id);
        if (effect == nullptr)
            return JS_UNDEFINED;

        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "typeId", string(ctx, "minecraft:" + std::string(getMobEffectName(effect->mId))));
        JS_SetPropertyStr(ctx, object, "amplifier", JS_NewInt32(ctx, effect->mAmplifier));
        JS_SetPropertyStr(ctx, object, "duration", JS_NewInt32(ctx, effect->mDuration));
        return object;
    }

    JSValue entityRemoveEffect(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        MobEffectId id;
        if (argc < 1 || !parseMobEffect(toString(ctx, argv[0]), id))
            return JS_FALSE;

        const bool removed = target.mActor->removeEffect(id);
        if (removed && target.mServerActor != nullptr)
            target.mApi->host().applyActorEffect(*target.mServerActor, (int32_t) id, 0, 0, false);
        return JS_NewBool(ctx, removed);
    }

    JSValue entitySetOnFire(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        int32_t seconds = 0;
        if (argc >= 1)
            JS_ToInt32(ctx, &seconds, argv[0]);
        entityOf(ctx, thisVal).mActor->setFireTicks(seconds * 20);
        return JS_TRUE;
    }

    JSValue entityExtinguishFire(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        entityOf(ctx, thisVal).mActor->extinguish();
        return JS_TRUE;
    }

    JSValue entityPlayAnimation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1)
            return JS_UNDEFINED;
        if (target.mPlayer != nullptr)
            target.mApi->host().playPlayerAnimation(*target.mPlayer, toString(ctx, argv[0]));
        else if (target.mServerActor != nullptr)
            target.mApi->host().playActorAnimation(*target.mServerActor, toString(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue entityRunCommand(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        const std::string line = argc >= 1 ? toString(ctx, argv[0]) : std::string();
        const int32_t success = target.mPlayer != nullptr ? target.mApi->runCommandAsPlayer(*target.mPlayer, line)
                                                          : target.mApi->runCommandAsConsole(line);

        JSValue result = target.mApi->makeInstance("CommandResult", new ScriptNative());
        JS_DefinePropertyValueStr(ctx, result, "successCount", JS_NewInt32(ctx, success), JS_PROP_ENUMERABLE);
        return result;
    }

    JSValue entityAddTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        return JS_NewBool(ctx, entityOf(ctx, thisVal).mActor->addTag(toString(ctx, argv[0])));
    }

    JSValue entityRemoveTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        return JS_NewBool(ctx, entityOf(ctx, thisVal).mActor->removeTag(toString(ctx, argv[0])));
    }

    JSValue entityHasTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        return JS_NewBool(ctx, entityOf(ctx, thisVal).mActor->hasTag(toString(ctx, argv[0])));
    }

    JSValue entityGetTags(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &tag: entityOf(ctx, thisVal).mActor->getTags())
            JS_SetPropertyUint32(ctx, array, index++, string(ctx, tag));
        return array;
    }

    JSValue entityMatches(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return JS_NewBool(ctx, argc >= 1 && matchesQuery(ctx, entityOf(ctx, thisVal), argv[0]));
    }

    JSValue entityGetProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1 || target.mServerActor == nullptr)
            return JS_UNDEFINED;

        const std::string name = toString(ctx, argv[0]);
        const ActorPropertyDescription *descriptor = target.mServerActor->findPropertyDescription(name);
        if (descriptor == nullptr)
            return JS_UNDEFINED;

        ServerActor &actor = *target.mServerActor;
        if (descriptor->mType == ActorPropertyDescription::Type::Float)
            return JS_NewFloat64(ctx, actor.getFloatProperty(name, descriptor->mDefaultFloat));
        if (descriptor->mType == ActorPropertyDescription::Type::Bool)
            return JS_NewBool(ctx, actor.getIntProperty(name, descriptor->mDefaultInt) != 0);
        if (descriptor->mType == ActorPropertyDescription::Type::Enum) {
            const int32_t index = actor.getIntProperty(name, descriptor->mDefaultInt);
            if (index >= 0 && index < (int32_t) descriptor->mEnumValues.size())
                return string(ctx, descriptor->mEnumValues[index]);
            return JS_UNDEFINED;
        }
        return JS_NewInt32(ctx, actor.getIntProperty(name, descriptor->mDefaultInt));
    }

    JSValue entitySetProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 2 || target.mServerActor == nullptr)
            return JS_UNDEFINED;

        const std::string name = toString(ctx, argv[0]);
        const ActorPropertyDescription *descriptor = target.mServerActor->findPropertyDescription(name);
        if (descriptor == nullptr)
            return JS_UNDEFINED;

        ServerActor &actor = *target.mServerActor;
        if (descriptor->mType == ActorPropertyDescription::Type::Float) {
            double value = 0.0;
            JS_ToFloat64(ctx, &value, argv[1]);
            actor.setFloatProperty(name, (float) value);
        } else if (descriptor->mType == ActorPropertyDescription::Type::Bool) {
            actor.setIntProperty(name, JS_ToBool(ctx, argv[1]) == 1 ? 1 : 0);
        } else if (descriptor->mType == ActorPropertyDescription::Type::Enum) {
            const std::string value = toString(ctx, argv[1]);
            for (size_t i = 0; i < descriptor->mEnumValues.size(); ++i) {
                if (descriptor->mEnumValues[i] == value)
                    actor.setIntProperty(name, (int32_t) i);
            }
        } else {
            int32_t value = 0;
            JS_ToInt32(ctx, &value, argv[1]);
            actor.setIntProperty(name, value);
        }

        target.mApi->host().syncActorProperties(actor);
        return JS_UNDEFINED;
    }

    JSValue entityGetViewDirection(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const Vector3f &rotation = entityOf(ctx, thisVal).mActor->getRotation();
        const float pitch = rotation.x * 0.017453292f;
        const float yaw = rotation.y * 0.017453292f;
        return vector3(ctx, -std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    }

    JSValue entityGetHeadLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        const Vector3f &position = target.mActor->getPosition();
        // TODO: eye height from the entity's collision box, players only for now
        const float eyes = target.mPlayer != nullptr ? 1.62f : 0.0f;
        return vector3(ctx, position.x, position.y + eyes, position.z);
    }

    JSValue entityGetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::unordered_map<std::string, DynamicPropertyValue> *store = dynamicPropertiesOf(entityOf(ctx, thisVal));
        if (store == nullptr || argc < 1)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, *store, toString(ctx, argv[0]));
    }

    JSValue entitySetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::unordered_map<std::string, DynamicPropertyValue> *store = dynamicPropertiesOf(entityOf(ctx, thisVal));
        if (store != nullptr && argc >= 1)
            dynamicPropertySet(ctx, *store, toString(ctx, argv[0]), argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    JSValue entityGetDynamicPropertyIds(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::unordered_map<std::string, DynamicPropertyValue> *store = dynamicPropertiesOf(entityOf(ctx, thisVal));
        return store == nullptr ? JS_NewArray(ctx) : dynamicPropertyIds(ctx, *store);
    }

    JSValue entityClearDynamicProperties(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::unordered_map<std::string, DynamicPropertyValue> *store = dynamicPropertiesOf(entityOf(ctx, thisVal));
        if (store != nullptr)
            store->clear();
        return JS_UNDEFINED;
    }

    std::string componentName(const std::string &id) {
        return id.find(':') == std::string::npos ? "minecraft:" + id : id;
    }

    // the components an entity of this kind carries; getComponent and hasComponent both answer from here
    JSValue makeEntityComponent(const EntityTarget &target, const std::string &requested) {
        ScriptApi *api = target.mApi;
        const std::string id = componentName(requested);

        if (target.mPlayer != nullptr && (id == "minecraft:inventory" || id == "minecraft:equippable")) {
            ScriptPlayerPart *part = new ScriptPlayerPart();
            part->mHandle = (uint32_t) target.mRef->mId;
            part->mComponentId = id;
            return api->makeInstance(id == "minecraft:inventory" ? "EntityInventoryComponent"
                                                                 : "EntityEquippableComponent", part);
        }

        const bool living = target.mPlayer != nullptr || target.mServerActor != nullptr;
        const std::vector<std::string> families = familiesOf(target);
        const CustomActorDefinition *definition = target.mServerActor == nullptr ? nullptr
                                                                                 : target.mServerActor->getDefinition();
        const char *className = nullptr;
        if (id == "minecraft:health" && living)
            className = "EntityHealthComponent";
        else if (id == "minecraft:type_family" && !families.empty())
            className = "EntityTypeFamilyComponent";
        else if (id == "minecraft:projectile" && definition != nullptr && definition->mIsProjectile)
            className = "EntityProjectileComponent";
        if (className == nullptr)
            return JS_UNDEFINED;

        ScriptEntityPart *part = new ScriptEntityPart();
        part->mEntity = *target.mRef;
        part->mComponentId = id;
        return api->makeInstance(className, part);
    }

    JSValue entityGetComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;
        return makeEntityComponent(entityOf(ctx, thisVal), toString(ctx, argv[0]));
    }

    JSValue entityHasComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        JSValue component = makeEntityComponent(entityOf(ctx, thisVal), toString(ctx, argv[0]));
        const bool has = !JS_IsUndefined(component);
        JS_FreeValue(ctx, component);
        return JS_NewBool(ctx, has);
    }

    JSValue entityGetComponents(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const char *id: {"minecraft:inventory", "minecraft:equippable", "minecraft:health",
                              "minecraft:type_family", "minecraft:projectile"}) {
            JSValue component = makeEntityComponent(target, id);
            if (JS_IsUndefined(component))
                continue;
            JS_SetPropertyUint32(ctx, array, index++, component);
        }
        return array;
    }

    ServerPlayer *playerOf(JSContext *ctx, JSValueConst thisVal) {
        return entityOf(ctx, thisVal).mPlayer;
    }

    JSValue playerGetName(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, playerOf(ctx, thisVal)->getName());
    }

    JSValue playerSendMessage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1)
            return JS_UNDEFINED;

        bool isJson = false;
        const std::string text = messageToRawText(ctx, argv[0], isJson);
        if (isJson)
            target.mApi->host().sendJsonMessage(*target.mPlayer, text);
        else
            target.mPlayer->sendMessage(text);
        return JS_UNDEFINED;
    }

    // 1.x names the game modes in lowercase, 2.0 capitalised
    const char *const GAME_MODES_1[] = {"survival", "creative", "adventure", "spectator"};
    const char *const GAME_MODES_2[] = {"Survival", "Creative", "Adventure", "Spectator"};

    JSValue playerGetGameMode(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        const char *const *names = target.mApi->isStable1() ? GAME_MODES_1 : GAME_MODES_2;
        const int32_t mode = target.mPlayer->getGameType();
        // GameType::Spectator is 6 on the wire
        const int32_t index = mode == 6 ? 3 : mode;
        return JS_NewString(ctx, index >= 0 && index <= 3 ? names[index] : names[0]);
    }

    JSValue playerSetGameMode(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1 || JS_IsUndefined(argv[0]))
            return JS_UNDEFINED;

        const char *const *names = target.mApi->isStable1() ? GAME_MODES_1 : GAME_MODES_2;
        const std::string mode = toString(ctx, argv[0]);
        for (int32_t i = 0; i < 4; ++i) {
            if (mode == names[i]) {
                target.mApi->host().setPlayerGameMode(*target.mPlayer, i == 3 ? 6 : i);
                return JS_UNDEFINED;
            }
        }
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    JSValue playerGetItemCooldown(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_NewInt32(ctx, 0);

        const std::string category = toString(ctx, argv[0]);
        for (const CustomItemDefinition &definition: CustomContentRegistry::getInstance().getItems()) {
            if (!definition.mCooldownCategory.empty() && definition.mCooldownCategory == category)
                return JS_NewInt32(ctx, definition.mCooldownTicks);
        }
        return JS_NewInt32(ctx, 0);
    }

    JSValue playerStartItemCooldown(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 2)
            return JS_UNDEFINED;

        int32_t duration = 0;
        JS_ToInt32(ctx, &duration, argv[1]);
        target.mApi->host().startPlayerItemCooldown(*target.mPlayer, toString(ctx, argv[0]), duration);
        return JS_UNDEFINED;
    }

    JSValue playerPlaySound(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1)
            return JS_UNDEFINED;

        Vector3f position = target.mPlayer->getPosition();
        float volume = 1.0f;
        float pitch = 1.0f;
        if (argc >= 2 && JS_IsObject(argv[1])) {
            JSValue locationValue = JS_GetPropertyStr(ctx, argv[1], "location");
            double x = 0.0, y = 0.0, z = 0.0;
            if (readVector3(ctx, locationValue, x, y, z))
                position = Vector3f((float) x, (float) y, (float) z);
            JS_FreeValue(ctx, locationValue);

            JSValue volumeValue = JS_GetPropertyStr(ctx, argv[1], "volume");
            JSValue pitchValue = JS_GetPropertyStr(ctx, argv[1], "pitch");
            double parsed = 1.0;
            if (!JS_IsUndefined(volumeValue) && JS_ToFloat64(ctx, &parsed, volumeValue) == 0)
                volume = (float) parsed;
            if (!JS_IsUndefined(pitchValue) && JS_ToFloat64(ctx, &parsed, pitchValue) == 0)
                pitch = (float) parsed;
            JS_FreeValue(ctx, volumeValue);
            JS_FreeValue(ctx, pitchValue);
        }

        target.mApi->host().playSoundFor(*target.mPlayer, toString(ctx, argv[0]), position, volume, pitch);
        return JS_UNDEFINED;
    }

    JSValue playerSpawnParticle(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        target.mApi->host().spawnParticleEffect(target.mApi->host().getLevelFor(*target.mPlayer),
                                                toString(ctx, argv[0]), Vector3f((float) x, (float) y, (float) z));
        return JS_UNDEFINED;
    }

    JSValue playerAddExperience(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        int32_t amount = 0;
        if (argc >= 1)
            JS_ToInt32(ctx, &amount, argv[0]);
        target.mPlayer->getExperience().addXp(amount);
        target.mApi->host().syncPlayerAttributes(*target.mPlayer);
        return JS_NewInt32(ctx, target.mPlayer->getExperience().getLifetimeTotalXp());
    }

    JSValue playerAddLevels(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        int32_t levels = 0;
        if (argc >= 1)
            JS_ToInt32(ctx, &levels, argv[0]);
        target.mPlayer->getExperience().addXpLevels(levels);
        target.mApi->host().syncPlayerAttributes(*target.mPlayer);
        return JS_NewInt32(ctx, target.mPlayer->getExperience().getXpLevel());
    }

    JSValue playerResetLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        target.mPlayer->getExperience().reset();
        target.mApi->host().syncPlayerAttributes(*target.mPlayer);
        return JS_UNDEFINED;
    }

    JSValue playerGetLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, playerOf(ctx, thisVal)->getExperience().getXpLevel());
    }

    JSValue playerGetSelectedSlot(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, playerOf(ctx, thisVal)->getInventory().getSelectedSlot());
    }

    JSValue makePlayerPart(JSContext *ctx, JSValueConst thisVal, const char *className) {
        const EntityTarget target = entityOf(ctx, thisVal);
        ScriptPlayerPart *part = new ScriptPlayerPart();
        part->mHandle = (uint32_t) target.mRef->mId;
        return target.mApi->makeInstance(className, part);
    }

    JSValue playerGetOnScreenDisplay(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return makePlayerPart(ctx, thisVal, "ScreenDisplay");
    }

    JSValue playerGetCamera(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return makePlayerPart(ctx, thisVal, "Camera");
    }

    JSValue playerGetInputPermissions(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return makePlayerPart(ctx, thisVal, "PlayerInputPermissions");
    }

    JSValue playerGetLocatorBar(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return makePlayerPart(ctx, thisVal, "LocatorBar");
    }

    ServerPlayer *partOwner(JSContext *ctx, JSValueConst thisVal, const ScriptPlayerPart **outPart = nullptr) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptPlayerPart *part = api->native<ScriptPlayerPart>(thisVal);
        if (outPart != nullptr)
            *outPart = part;
        return part == nullptr ? nullptr : api->resolvePlayerByHandle(part->mHandle);
    }

    JSValue screenSetActionBar(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player != nullptr && argc >= 1) {
            bool isJson = false;
            const std::string text = messageToRawText(ctx, argv[0], isJson);
            ScriptApi::fromContext(ctx)->host().sendActionBar(*player, text, isJson);
        }
        return JS_UNDEFINED;
    }

    JSValue screenSetTitle(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player != nullptr && argc >= 1) {
            bool isJson = false;
            const std::string text = messageToRawText(ctx, argv[0], isJson);
            ScriptApi::fromContext(ctx)->host().sendTitle(*player, text, isJson);
        }
        return JS_UNDEFINED;
    }

    JSValue partGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, partOwner(ctx, thisVal) != nullptr);
    }

    // TODO: subtitle, camera presets and fades, input permissions and the locator bar do nothing yet
    JSValue partNoop(JSContext *, JSValueConst, int, JSValueConst *) {
        return JS_UNDEFINED;
    }

    JSValue inputPermissionEnabled(JSContext *, JSValueConst, int, JSValueConst *) {
        return JS_TRUE;
    }

    JSValue cameraClear(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player != nullptr)
            ScriptApi::fromContext(ctx)->host().clearPlayerCamera(*player);
        return JS_UNDEFINED;
    }

    JSValue containerGetSize(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewInt32(ctx, PlayerInventory::CONTAINER_SIZE);
    }

    JSValue containerAddItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = partOwner(ctx, thisVal);
        const ScriptItemRef *item = argc >= 1 ? api->native<ScriptItemRef>(argv[0]) : nullptr;
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (player != nullptr)
            api->giveItem(*player, item->mItem->mTypeId, item->mItem->mAmount);
        return JS_UNDEFINED;
    }

    JSValue itemFromStack(ScriptApi *api, const ItemStack &stack) {
        if (stack.isAir() || stack.mDefinition == nullptr)
            return JS_UNDEFINED;

        JSValue jsItem = api->makeItemStack(stack.mDefinition->getIdentifier(), stack.mCount);
        const ScriptItemRef *ref = api->native<ScriptItemRef>(jsItem);
        ref->mItem->mDamage = stack.mDamage;
        if (stack.mTag.isCompound()) {
            const Tag *dynamicProperties = stack.mTag.get("DynamicProperties");
            if (dynamicProperties != nullptr)
                deserializeDynamicProperties(*dynamicProperties, ref->mItem->mDynamicProperties);
        }
        return jsItem;
    }

    JSValue containerGetItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = partOwner(ctx, thisVal);
        int32_t slot = 0;
        if (player == nullptr || argc < 1 || JS_ToInt32(ctx, &slot, argv[0]) != 0)
            return JS_UNDEFINED;
        if (slot < 0 || slot >= PlayerInventory::CONTAINER_SIZE)
            return JS_UNDEFINED;
        return itemFromStack(api, player->getInventory().getItem(slot));
    }

    JSValue containerSetItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = partOwner(ctx, thisVal);
        int32_t slot = 0;
        if (player == nullptr || argc < 1 || JS_ToInt32(ctx, &slot, argv[0]) != 0)
            return JS_UNDEFINED;

        const ScriptItemRef *item = argc >= 2 ? api->native<ScriptItemRef>(argv[1]) : nullptr;
        if (item == nullptr) {
            api->host().setContainerSlot(*player, slot, std::string(), 0, Tag::ofCompound());
            return JS_UNDEFINED;
        }
        api->host().setContainerSlot(*player, slot, item->mItem->mTypeId, item->mItem->mAmount,
                                     serializeDynamicProperties(item->mItem->mDynamicProperties));
        return JS_UNDEFINED;
    }

    JSValue containerGetEmptySlotsCount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        int32_t empty = 0;
        for (int slot = 0; player != nullptr && slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
            if (player->getInventory().getItem(slot).isAir())
                empty++;
        }
        return JS_NewInt32(ctx, empty);
    }

    JSValue inventoryGetContainer(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptPlayerPart *part = api->native<ScriptPlayerPart>(thisVal);
        ScriptPlayerPart *container = new ScriptPlayerPart();
        container->mHandle = part->mHandle;
        return api->makeInstance("Container", container);
    }

    const ItemStack &equipmentStack(const PlayerInventory &inventory, const std::string &slot) {
        if (slot == "Head")
            return inventory.getArmor(PlayerInventory::ARMOR_HEAD);
        if (slot == "Chest")
            return inventory.getArmor(PlayerInventory::ARMOR_TORSO);
        if (slot == "Legs")
            return inventory.getArmor(PlayerInventory::ARMOR_LEGS);
        if (slot == "Feet")
            return inventory.getArmor(PlayerInventory::ARMOR_FEET);
        if (slot == "Offhand")
            return inventory.getOffhand();
        return inventory.getItemInHand();
    }

    JSValue equippableGetEquipment(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player == nullptr || argc < 1)
            return JS_UNDEFINED;
        return itemFromStack(ScriptApi::fromContext(ctx), equipmentStack(player->getInventory(),
                                                                          toString(ctx, argv[0])));
    }

    void setEquipment(JSContext *ctx, ServerPlayer &player, const std::string &slot, JSValueConst value) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptItemRef *item = api->native<ScriptItemRef>(value);
        if (item == nullptr) {
            api->host().setPlayerEquipment(player, slot, std::string(), 0, 0, Tag::ofCompound());
            return;
        }
        api->host().setPlayerEquipment(player, slot, item->mItem->mTypeId, item->mItem->mAmount,
                                       item->mItem->mDamage, serializeDynamicProperties(item->mItem->mDynamicProperties));
    }

    JSValue equippableSetEquipment(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player == nullptr || argc < 1)
            return JS_FALSE;
        setEquipment(ctx, *player, toString(ctx, argv[0]), argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_TRUE;
    }

    JSValue equippableGetEquipmentSlot(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptPlayerPart *part = api->native<ScriptPlayerPart>(thisVal);
        ScriptPlayerPart *slot = new ScriptPlayerPart();
        slot->mHandle = part->mHandle;
        slot->mSlot = argc >= 1 ? toString(ctx, argv[0]) : std::string("Mainhand");
        return api->makeInstance("ContainerSlot", slot);
    }

    JSValue slotGetItem(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptPlayerPart *part = nullptr;
        ServerPlayer *player = partOwner(ctx, thisVal, &part);
        if (player == nullptr)
            return JS_UNDEFINED;
        return itemFromStack(ScriptApi::fromContext(ctx), equipmentStack(player->getInventory(), part->mSlot));
    }

    JSValue slotHasItem(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptPlayerPart *part = nullptr;
        ServerPlayer *player = partOwner(ctx, thisVal, &part);
        if (player == nullptr)
            return JS_FALSE;
        const ItemStack &stack = equipmentStack(player->getInventory(), part->mSlot);
        return JS_NewBool(ctx, !stack.isAir() && stack.mDefinition != nullptr);
    }

    JSValue slotSetItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const ScriptPlayerPart *part = nullptr;
        ServerPlayer *player = partOwner(ctx, thisVal, &part);
        if (player != nullptr)
            setEquipment(ctx, *player, part->mSlot, argc >= 1 ? argv[0] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    Actor *componentEntity(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptEntityPart *part = api->native<ScriptEntityPart>(thisVal);
        return part == nullptr ? nullptr : api->resolveEntity(part->mEntity);
    }

    JSValue componentGetTypeId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (const ScriptEntityPart *entity = api->native<ScriptEntityPart>(thisVal))
            return string(ctx, entity->mComponentId);
        if (const ScriptPlayerPart *player = api->native<ScriptPlayerPart>(thisVal))
            return string(ctx, player->mComponentId);
        if (const ScriptItemRef *item = api->native<ScriptItemRef>(thisVal))
            return string(ctx, item->mComponentId);
        return JS_UNDEFINED;
    }

    JSValue componentGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (api->native<ScriptEntityPart>(thisVal) != nullptr)
            return JS_NewBool(ctx, componentEntity(ctx, thisVal) != nullptr);
        if (api->native<ScriptPlayerPart>(thisVal) != nullptr)
            return JS_NewBool(ctx, partOwner(ctx, thisVal) != nullptr);
        return JS_TRUE;
    }

    JSValue healthGetValue(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        Actor *actor = componentEntity(ctx, thisVal);
        if (actor == nullptr)
            return JS_NewFloat64(ctx, 0.0);
        return JS_NewFloat64(ctx, magic == 0 ? actor->getHealth() : actor->getMaxHealth());
    }

    JSValue healthSetCurrent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        Actor *actor = componentEntity(ctx, thisVal);
        double value = 0.0;
        if (actor == nullptr || argc < 1 || JS_ToFloat64(ctx, &value, argv[0]) != 0)
            return JS_FALSE;
        actor->setHealth((float) value);
        return JS_TRUE;
    }

    JSValue healthResetToMax(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        Actor *actor = componentEntity(ctx, thisVal);
        if (actor != nullptr)
            actor->setHealth(actor->getMaxHealth());
        return JS_UNDEFINED;
    }

    std::vector<std::string> componentFamilies(JSContext *ctx, JSValueConst thisVal) {
        ServerActor *actor = dynamic_cast<ServerActor *>(componentEntity(ctx, thisVal));
        if (actor == nullptr || actor->getDefinition() == nullptr)
            return std::vector<std::string>();
        return actor->getDefinition()->mTypeFamilies;
    }

    JSValue typeFamilyHas(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        const std::vector<std::string> families = componentFamilies(ctx, thisVal);
        const std::string wanted = toString(ctx, argv[0]);
        return JS_NewBool(ctx, std::find(families.begin(), families.end(), wanted) != families.end());
    }

    JSValue typeFamilyGetAll(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &family: componentFamilies(ctx, thisVal))
            JS_SetPropertyUint32(ctx, array, index++, string(ctx, family));
        return array;
    }

    JSValue projectileGetOwner(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerActor *actor = dynamic_cast<ServerActor *>(componentEntity(ctx, thisVal));
        if (actor == nullptr)
            return JS_UNDEFINED;
        if (actor->hasOwnerPlayer()) {
            ServerPlayer *owner = api->resolvePlayerByHandle(actor->getOwnerPlayerHandle());
            return owner == nullptr ? JS_UNDEFINED : api->makePlayer(*owner);
        }
        ServerActor *owner = actor->getOwnerUniqueId() >= 0 ? api->host().getActor(actor->getOwnerUniqueId()) : nullptr;
        return owner == nullptr ? JS_UNDEFINED : api->makeActor(*owner);
    }

    JSValue projectileSetOwner(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerActor *actor = dynamic_cast<ServerActor *>(componentEntity(ctx, thisVal));
        const ScriptEntityRef *owner = argc >= 1 ? api->native<ScriptEntityRef>(argv[0]) : nullptr;
        if (actor == nullptr || owner == nullptr)
            return JS_UNDEFINED;
        if (owner->mKind == ScriptEntityRef::Kind::Player)
            actor->setOwnerPlayerHandle((uint32_t) owner->mId);
        else if (owner->mKind == ScriptEntityRef::Kind::Actor)
            actor->setOwnerUniqueId(owner->mId);
        return JS_UNDEFINED;
    }

    JSValue projectileShoot(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = dynamic_cast<ServerActor *>(componentEntity(ctx, thisVal));
        double x = 0.0, y = 0.0, z = 0.0;
        if (actor == nullptr || argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_UNDEFINED;
        actor->setMotion(Vector3f((float) x, (float) y, (float) z));
        actor->setProjectile(true);
        return JS_UNDEFINED;
    }
}

void ScriptApi::_bindEntity() {
    JSValue entity = defineClass("Entity");
    property(entity, "Entity", "typeId", entityGetTypeId);
    property(entity, "Entity", "id", entityGetId);
    property(entity, "Entity", "isValid", entityGetIsValid);
    property(entity, "Entity", "location", entityGetLocation);
    property(entity, "Entity", "dimension", entityGetDimension);
    property(entity, "Entity", "nameTag", entityGetNameTag, entitySetNameTag);
    propertyMagic(entity, "Entity", "isSneaking", entityGetFlag, 0);
    propertyMagic(entity, "Entity", "isSprinting", entityGetFlag, 1);

    method(entity, "Entity", "remove", entityRemove, 0);
    method(entity, "Entity", "teleport", entityTeleport, 2);
    method(entity, "Entity", "getRotation", entityGetRotation, 0);
    method(entity, "Entity", "setRotation", entitySetRotation, 1);
    method(entity, "Entity", "getVelocity", entityGetVelocity, 0);
    method(entity, "Entity", "clearVelocity", entityClearVelocity, 0);
    method(entity, "Entity", "applyImpulse", entityApplyImpulse, 1);
    method(entity, "Entity", "applyKnockback", entityApplyKnockback, isStable1() ? 4 : 2);
    method(entity, "Entity", "applyDamage", entityApplyDamage, 2);
    method(entity, "Entity", "kill", entityKill, 0);
    method(entity, "Entity", "addEffect", entityAddEffect, 3);
    method(entity, "Entity", "getEffect", entityGetEffect, 1);
    method(entity, "Entity", "removeEffect", entityRemoveEffect, 1);
    method(entity, "Entity", "setOnFire", entitySetOnFire, 2);
    method(entity, "Entity", "extinguishFire", entityExtinguishFire, 1);
    method(entity, "Entity", "playAnimation", entityPlayAnimation, 2);
    method(entity, "Entity", "runCommand", entityRunCommand, 1);
    method(entity, "Entity", "addTag", entityAddTag, 1);
    method(entity, "Entity", "removeTag", entityRemoveTag, 1);
    method(entity, "Entity", "hasTag", entityHasTag, 1);
    method(entity, "Entity", "getTags", entityGetTags, 0);
    method(entity, "Entity", "matches", entityMatches, 1);
    method(entity, "Entity", "getProperty", entityGetProperty, 1);
    method(entity, "Entity", "setProperty", entitySetProperty, 2);
    method(entity, "Entity", "getViewDirection", entityGetViewDirection, 0);
    method(entity, "Entity", "getHeadLocation", entityGetHeadLocation, 0);
    method(entity, "Entity", "getComponent", entityGetComponent, 1);
    method(entity, "Entity", "hasComponent", entityHasComponent, 1);
    method(entity, "Entity", "getComponents", entityGetComponents, 0);
    method(entity, "Entity", "getDynamicProperty", entityGetDynamicProperty, 1);
    method(entity, "Entity", "setDynamicProperty", entitySetDynamicProperty, 2);
    method(entity, "Entity", "getDynamicPropertyIds", entityGetDynamicPropertyIds, 0);
    method(entity, "Entity", "clearDynamicProperties", entityClearDynamicProperties, 0);

    defineClass("CommandResult");
}

void ScriptApi::_bindPlayer() {
    JSValue player = defineClass("Player");
    property(player, "Player", "name", playerGetName);
    property(player, "Player", "level", playerGetLevel);
    property(player, "Player", "selectedSlotIndex", playerGetSelectedSlot);
    property(player, "Player", "onScreenDisplay", playerGetOnScreenDisplay);
    property(player, "Player", "camera", playerGetCamera);
    property(player, "Player", "inputPermissions", playerGetInputPermissions);
    property(player, "Player", "locatorBar", playerGetLocatorBar);

    method(player, "Player", "sendMessage", playerSendMessage, 1);
    method(player, "Player", "getGameMode", playerGetGameMode, 0);
    method(player, "Player", "setGameMode", playerSetGameMode, 1);
    method(player, "Player", "getItemCooldown", playerGetItemCooldown, 1);
    method(player, "Player", "startItemCooldown", playerStartItemCooldown, 2);
    method(player, "Player", "playSound", playerPlaySound, 2);
    method(player, "Player", "spawnParticle", playerSpawnParticle, 3);
    method(player, "Player", "addExperience", playerAddExperience, 1);
    method(player, "Player", "addLevels", playerAddLevels, 1);
    method(player, "Player", "resetLevel", playerResetLevel, 0);

    JSValue screen = defineClass("ScreenDisplay");
    property(screen, "ScreenDisplay", "isValid", partGetIsValid);
    method(screen, "ScreenDisplay", "setActionBar", screenSetActionBar, 1);
    method(screen, "ScreenDisplay", "setTitle", screenSetTitle, 2);
    method(screen, "ScreenDisplay", "updateSubtitle", partNoop, 1);

    JSValue camera = defineClass("Camera");
    property(camera, "Camera", "isValid", partGetIsValid);
    method(camera, "Camera", "clear", cameraClear, 0);
    method(camera, "Camera", "setCamera", partNoop, 2);
    method(camera, "Camera", "fade", partNoop, 1);

    JSValue permissions = defineClass("PlayerInputPermissions");
    method(permissions, "PlayerInputPermissions", "setPermissionCategory", partNoop, 2);
    method(permissions, "PlayerInputPermissions", "isPermissionCategoryEnabled", inputPermissionEnabled, 1);

    JSValue locator = defineClass("LocatorBar");
    method(locator, "LocatorBar", "addWaypoint", partNoop, 1);
    method(locator, "LocatorBar", "removeWaypoint", partNoop, 1);
    method(locator, "LocatorBar", "removeAllWaypoints", partNoop, 0);
}

void ScriptApi::_bindComponents() {
    JSValue component = defineClass("Component");
    property(component, "Component", "typeId", componentGetTypeId);
    property(component, "Component", "isValid", componentGetIsValid);

    JSValue inventory = defineClass("EntityInventoryComponent");
    property(inventory, "EntityInventoryComponent", "container", inventoryGetContainer);

    JSValue container = defineClass("Container");
    property(container, "Container", "size", containerGetSize);
    property(container, "Container", "emptySlotsCount", containerGetEmptySlotsCount);
    property(container, "Container", "isValid", partGetIsValid);
    method(container, "Container", "addItem", containerAddItem, 1);
    method(container, "Container", "getItem", containerGetItem, 1);
    method(container, "Container", "setItem", containerSetItem, 2);

    JSValue equippable = defineClass("EntityEquippableComponent");
    method(equippable, "EntityEquippableComponent", "getEquipment", equippableGetEquipment, 1);
    method(equippable, "EntityEquippableComponent", "setEquipment", equippableSetEquipment, 2);
    method(equippable, "EntityEquippableComponent", "getEquipmentSlot", equippableGetEquipmentSlot, 1);

    JSValue slot = defineClass("ContainerSlot");
    property(slot, "ContainerSlot", "isValid", partGetIsValid);
    method(slot, "ContainerSlot", "getItem", slotGetItem, 0);
    method(slot, "ContainerSlot", "hasItem", slotHasItem, 0);
    method(slot, "ContainerSlot", "setItem", slotSetItem, 1);

    JSValue attribute = defineClass("EntityAttributeComponent");
    propertyMagic(attribute, "EntityAttributeComponent", "currentValue", healthGetValue, 0);
    propertyMagic(attribute, "EntityAttributeComponent", "effectiveMax", healthGetValue, 1);
    propertyMagic(attribute, "EntityAttributeComponent", "defaultValue", healthGetValue, 1);
    method(attribute, "EntityAttributeComponent", "setCurrentValue", healthSetCurrent, 1);
    method(attribute, "EntityAttributeComponent", "resetToMaxValue", healthResetToMax, 0);
    defineClass("EntityHealthComponent");

    JSValue family = defineClass("EntityTypeFamilyComponent");
    method(family, "EntityTypeFamilyComponent", "hasTypeFamily", typeFamilyHas, 1);
    method(family, "EntityTypeFamilyComponent", "getTypeFamilies", typeFamilyGetAll, 0);

    JSValue projectile = defineClass("EntityProjectileComponent");
    property(projectile, "EntityProjectileComponent", "owner", projectileGetOwner, projectileSetOwner);
    method(projectile, "EntityProjectileComponent", "shoot", projectileShoot, 2);
}
