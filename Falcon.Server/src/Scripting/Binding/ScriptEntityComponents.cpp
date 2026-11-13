#include "Scripting/Binding/ScriptApi.h"

#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Misc/NpcActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/RideSystem.h"
#include "World/Actor/ServerActor.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

using namespace ScriptValues;

namespace {
    // how getComponent decides an entity has a component and what backs it
    enum class Backing {
        // the active definition component, read field by field
        Definition,
        // an attribute of the entity, or the definition component when the attribute is missing
        Attribute,
        TypeFamily,
        Projectile,
        OnFire,
        Riding,
        Item,
        Tamed,
        Npc,
    };

    struct ComponentClass {
        const char *mId;
        const char *mClass;
        Backing mBacking;
    };

    const ComponentClass COMPONENTS[] = {
            {"minecraft:addrider", "EntityAddRiderComponent", Backing::Definition},
            {"minecraft:ageable", "EntityAgeableComponent", Backing::Definition},
            {"minecraft:breathable", "EntityBreathableComponent", Backing::Definition},
            {"minecraft:can_climb", "EntityCanClimbComponent", Backing::Definition},
            {"minecraft:can_fly", "EntityCanFlyComponent", Backing::Definition},
            {"minecraft:can_power_jump", "EntityCanPowerJumpComponent", Backing::Definition},
            {"minecraft:color", "EntityColorComponent", Backing::Definition},
            {"minecraft:color2", "EntityColor2Component", Backing::Definition},
            {"minecraft:fire_immune", "EntityFireImmuneComponent", Backing::Definition},
            {"minecraft:floats_in_liquid", "EntityFloatsInLiquidComponent", Backing::Definition},
            {"minecraft:flying_speed", "EntityFlyingSpeedComponent", Backing::Definition},
            {"minecraft:friction_modifier", "EntityFrictionModifierComponent", Backing::Definition},
            {"minecraft:ground_offset", "EntityGroundOffsetComponent", Backing::Definition},
            {"minecraft:healable", "EntityHealableComponent", Backing::Definition},
            {"minecraft:health", "EntityHealthComponent", Backing::Attribute},
            {"minecraft:is_baby", "EntityIsBabyComponent", Backing::Definition},
            {"minecraft:is_charged", "EntityIsChargedComponent", Backing::Definition},
            {"minecraft:is_chested", "EntityIsChestedComponent", Backing::Definition},
            {"minecraft:is_dyeable", "EntityIsDyeableComponent", Backing::Definition},
            {"minecraft:is_hidden_when_invisible", "EntityIsHiddenWhenInvisibleComponent", Backing::Definition},
            {"minecraft:is_ignited", "EntityIsIgnitedComponent", Backing::Definition},
            {"minecraft:is_illager_captain", "EntityIsIllagerCaptainComponent", Backing::Definition},
            {"minecraft:is_saddled", "EntityIsSaddledComponent", Backing::Definition},
            {"minecraft:is_shaking", "EntityIsShakingComponent", Backing::Definition},
            {"minecraft:is_sheared", "EntityIsShearedComponent", Backing::Definition},
            {"minecraft:is_stackable", "EntityIsStackableComponent", Backing::Definition},
            {"minecraft:is_stunned", "EntityIsStunnedComponent", Backing::Definition},
            {"minecraft:is_tamed", "EntityIsTamedComponent", Backing::Tamed},
            {"minecraft:item", "EntityItemComponent", Backing::Item},
            {"minecraft:lava_movement", "EntityLavaMovementComponent", Backing::Attribute},
            {"minecraft:leashable", "EntityLeashableComponent", Backing::Definition},
            {"minecraft:mark_variant", "EntityMarkVariantComponent", Backing::Definition},
            {"minecraft:movement", "EntityMovementComponent", Backing::Attribute},
            {"minecraft:movement.amphibious", "EntityMovementAmphibiousComponent", Backing::Definition},
            {"minecraft:movement.basic", "EntityMovementBasicComponent", Backing::Definition},
            {"minecraft:movement.fly", "EntityMovementFlyComponent", Backing::Definition},
            {"minecraft:movement.generic", "EntityMovementGenericComponent", Backing::Definition},
            {"minecraft:movement.glide", "EntityMovementGlideComponent", Backing::Definition},
            {"minecraft:movement.hover", "EntityMovementHoverComponent", Backing::Definition},
            {"minecraft:movement.jump", "EntityMovementJumpComponent", Backing::Definition},
            {"minecraft:movement.skip", "EntityMovementSkipComponent", Backing::Definition},
            {"minecraft:movement.sway", "EntityMovementSwayComponent", Backing::Definition},
            {"minecraft:navigation.climb", "EntityNavigationClimbComponent", Backing::Definition},
            {"minecraft:navigation.float", "EntityNavigationFloatComponent", Backing::Definition},
            {"minecraft:navigation.fly", "EntityNavigationFlyComponent", Backing::Definition},
            {"minecraft:navigation.generic", "EntityNavigationGenericComponent", Backing::Definition},
            {"minecraft:navigation.hover", "EntityNavigationHoverComponent", Backing::Definition},
            {"minecraft:navigation.walk", "EntityNavigationWalkComponent", Backing::Definition},
            {"minecraft:npc", "EntityNpcComponent", Backing::Npc},
            {"minecraft:onfire", "EntityOnFireComponent", Backing::OnFire},
            {"minecraft:player.exhaustion", "EntityExhaustionComponent", Backing::Attribute},
            {"minecraft:player.hunger", "EntityHungerComponent", Backing::Attribute},
            {"minecraft:player.saturation", "EntitySaturationComponent", Backing::Attribute},
            {"minecraft:projectile", "EntityProjectileComponent", Backing::Projectile},
            {"minecraft:push_through", "EntityPushThroughComponent", Backing::Definition},
            {"minecraft:rideable", "EntityRideableComponent", Backing::Definition},
            {"minecraft:riding", "EntityRidingComponent", Backing::Riding},
            {"minecraft:scale", "EntityScaleComponent", Backing::Definition},
            {"minecraft:skin_id", "EntitySkinIdComponent", Backing::Definition},
            {"minecraft:strength", "EntityStrengthComponent", Backing::Definition},
            {"minecraft:tameable", "EntityTameableComponent", Backing::Definition},
            {"minecraft:tamemount", "EntityTameMountComponent", Backing::Definition},
            {"minecraft:type_family", "EntityTypeFamilyComponent", Backing::TypeFamily},
            {"minecraft:underwater_movement", "EntityUnderwaterMovementComponent", Backing::Attribute},
            {"minecraft:variant", "EntityVariantComponent", Backing::Definition},
            {"minecraft:wants_jockey", "EntityWantsJockeyComponent", Backing::Definition},
    };

    const ComponentClass *findComponent(const std::string &id) {
        for (const ComponentClass &component: COMPONENTS) {
            if (id == component.mId)
                return &component;
        }
        return nullptr;
    }

    const json::Value *playerDefinitionComponent(const std::string &id) {
        const json::Value *definition = EntityDefinitions::find("minecraft:player");
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        return components == nullptr ? nullptr : components->get(id);
    }

    const json::Value *definitionOf(Actor &actor, const std::string &id) {
        if (MobActor *mob = dynamic_cast<MobActor *>(&actor))
            return mob->getComponent(id);
        if (actor.isPlayer())
            return playerDefinitionComponent(id);
        return nullptr;
    }

    const AttributeData *attributeOf(const Actor &actor, const std::string &id) {
        for (const AttributeData &attribute: actor.getAttributes().getAll()) {
            if (attribute.mName == id)
                return &attribute;
        }
        return nullptr;
    }
}

std::vector<std::string> ScriptApi::entityFamilies(Actor &actor) {
    std::vector<std::string> families;
    const json::Value *component = definitionOf(actor, "minecraft:type_family");
    const json::Value *family = component == nullptr ? nullptr : component->get("family");
    if (family != nullptr) {
        for (const std::unique_ptr<json::Value> &entry: family->mArray)
            families.push_back(entry->string());
        return families;
    }
    ServerActor *serverActor = dynamic_cast<ServerActor *>(&actor);
    if (serverActor != nullptr && serverActor->getDefinition() != nullptr)
        return serverActor->getDefinition()->mTypeFamilies;
    return families;
}

namespace {
    bool hasComponent(Actor &actor, const ComponentClass &component) {
        ServerActor *serverActor = dynamic_cast<ServerActor *>(&actor);
        switch (component.mBacking) {
            case Backing::Definition:
                return definitionOf(actor, component.mId) != nullptr;
            case Backing::Attribute:
                return attributeOf(actor, component.mId) != nullptr || definitionOf(actor, component.mId) != nullptr;
            case Backing::TypeFamily:
                return !ScriptApi::entityFamilies(actor).empty();
            case Backing::Projectile:
                return definitionOf(actor, component.mId) != nullptr
                       || (serverActor != nullptr && serverActor->isProjectile());
            case Backing::OnFire:
                return actor.isOnFire();
            case Backing::Riding:
                return actor.isRiding();
            case Backing::Item:
                return dynamic_cast<ItemActor *>(&actor) != nullptr;
            case Backing::Tamed: {
                MobActor *mob = dynamic_cast<MobActor *>(&actor);
                return definitionOf(actor, component.mId) != nullptr || (mob != nullptr && mob->isTamed());
            }
            case Backing::Npc:
                return dynamic_cast<NpcActor *>(&actor) != nullptr;
        }
        return false;
    }

    struct ComponentTarget {
        ScriptApi *mApi = nullptr;
        const ScriptEntityPart *mPart = nullptr;
        Actor *mActor = nullptr;
    };

    // every component member but isValid: the entity must still exist
    bool componentOf(JSContext *ctx, JSValueConst thisVal, ComponentTarget &target) {
        target.mApi = ScriptApi::fromContext(ctx);
        target.mPart = target.mApi->native<ScriptEntityPart>(thisVal);
        target.mActor = target.mPart == nullptr ? nullptr : target.mApi->resolveEntity(target.mPart->mEntity);
        if (target.mActor != nullptr)
            return true;
        target.mApi->throwError("InvalidEntityError", "Failed to get the component: the entity is invalid.");
        return false;
    }

    const json::Value *fieldOf(const ComponentTarget &target, const char *key) {
        const json::Value *component = definitionOf(*target.mActor, target.mPart->mComponentId);
        return component == nullptr ? nullptr : component->get(key);
    }

    // a definition field as it is; undefined when the definition leaves it out
    JSValue jsonToJs(JSContext *ctx, const json::Value *value) {
        if (value == nullptr)
            return JS_UNDEFINED;
        if (value->mType == json::Value::Type::Boolean)
            return JS_NewBool(ctx, value->mBoolean);
        if (value->isNumber())
            return JS_NewFloat64(ctx, value->mNumber);
        if (value->isString())
            return string(ctx, value->mString);
        if (value->isArray()) {
            JSValue array = JS_NewArray(ctx);
            uint32_t index = 0;
            for (const std::unique_ptr<json::Value> &element: value->mArray)
                JS_SetPropertyUint32(ctx, array, index++, jsonToJs(ctx, element.get()));
            return array;
        }
        return JS_UNDEFINED;
    }

    struct DefinitionField {
        const char *mClass;
        const char *mProperty;
        const char *mKey;
    };

    // official property, definition key
    const DefinitionField FIELDS[] = {
            {"EntityAddRiderComponent", "entityType", "entity_type"},
            {"EntityAddRiderComponent", "spawnEvent", "spawn_event"},
            {"EntityAgeableComponent", "duration", "duration"},
            {"EntityAgeableComponent", "transformToItem", "transform_to_item"},
            {"EntityBreathableComponent", "breathesAir", "breathes_air"},
            {"EntityBreathableComponent", "breathesLava", "breathes_lava"},
            {"EntityBreathableComponent", "breathesSolids", "breathes_solids"},
            {"EntityBreathableComponent", "breathesWater", "breathes_water"},
            {"EntityBreathableComponent", "generatesBubbles", "generates_bubbles"},
            {"EntityBreathableComponent", "inhaleTime", "inhale_time"},
            {"EntityBreathableComponent", "suffocateTime", "suffocate_time"},
            {"EntityBreathableComponent", "totalSupply", "total_supply"},
            {"EntityColorComponent", "value", "value"},
            {"EntityColor2Component", "value", "value"},
            {"EntityFlyingSpeedComponent", "value", "value"},
            {"EntityFrictionModifierComponent", "value", "value"},
            {"EntityGroundOffsetComponent", "value", "value"},
            {"EntityHealableComponent", "forceUse", "force_use"},
            {"EntityLeashableComponent", "canBeStolen", "can_be_stolen"},
            {"EntityLeashableComponent", "hardDistance", "hard_distance"},
            {"EntityLeashableComponent", "maxDistance", "max_distance"},
            {"EntityLeashableComponent", "softDistance", "soft_distance"},
            {"EntityMarkVariantComponent", "value", "value"},
            {"EntityBaseMovementComponent", "maxTurn", "max_turn"},
            {"EntityMovementGlideComponent", "speedWhenTurning", "speed_when_turning"},
            {"EntityMovementGlideComponent", "startSpeed", "start_speed"},
            {"EntityMovementSwayComponent", "swayAmplitude", "sway_amplitude"},
            {"EntityMovementSwayComponent", "swayFrequency", "sway_frequency"},
            {"EntityNavigationComponent", "avoidDamageBlocks", "avoid_damage_blocks"},
            {"EntityNavigationComponent", "avoidPortals", "avoid_portals"},
            {"EntityNavigationComponent", "avoidSun", "avoid_sun"},
            {"EntityNavigationComponent", "avoidWater", "avoid_water"},
            {"EntityNavigationComponent", "canBreach", "can_breach"},
            {"EntityNavigationComponent", "canBreakDoors", "can_break_doors"},
            {"EntityNavigationComponent", "canFloat", "can_float"},
            {"EntityNavigationComponent", "canJump", "can_jump"},
            {"EntityNavigationComponent", "canOpenDoors", "can_open_doors"},
            {"EntityNavigationComponent", "canOpenIronDoors", "can_open_iron_doors"},
            {"EntityNavigationComponent", "canPassDoors", "can_pass_doors"},
            {"EntityNavigationComponent", "canPathFromAir", "can_path_from_air"},
            {"EntityNavigationComponent", "canPathOverLava", "can_path_over_lava"},
            {"EntityNavigationComponent", "canPathOverWater", "can_path_over_water"},
            {"EntityNavigationComponent", "canSink", "can_sink"},
            {"EntityNavigationComponent", "canSwim", "can_swim"},
            {"EntityNavigationComponent", "canWalk", "can_walk"},
            {"EntityNavigationComponent", "canWalkInLava", "can_walk_in_lava"},
            {"EntityNavigationComponent", "isAmphibious", "is_amphibious"},
            {"EntityPushThroughComponent", "value", "value"},
            {"EntityRideableComponent", "controllingSeat", "controlling_seat"},
            {"EntityRideableComponent", "crouchingSkipInteract", "crouching_skip_interact"},
            {"EntityRideableComponent", "interactText", "interact_text"},
            {"EntityRideableComponent", "passengerMaxWidth", "passenger_max_width"},
            {"EntityRideableComponent", "pullInEntities", "pull_in_entities"},
            {"EntityRideableComponent", "riderCanInteract", "rider_can_interact"},
            {"EntityRideableComponent", "seatCount", "seat_count"},
            {"EntityScaleComponent", "value", "value"},
            {"EntitySkinIdComponent", "value", "value"},
            {"EntityStrengthComponent", "max", "max"},
            {"EntityStrengthComponent", "value", "value"},
            {"EntityTameableComponent", "probability", "probability"},
    };

    JSValue fieldGetter(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        return jsonToJs(ctx, fieldOf(target, FIELDS[magic].mKey));
    }

    JSValue variantGetValue(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        MobActor *mob = dynamic_cast<MobActor *>(target.mActor);
        if (mob != nullptr)
            return JS_NewInt32(ctx, mob->getVariant());
        return jsonToJs(ctx, fieldOf(target, "value"));
    }

    JSValue listGetter(JSContext *ctx, JSValueConst thisVal, const char *key) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        JSValue list = jsonToJs(ctx, fieldOf(target, key));
        return JS_IsUndefined(list) ? JS_NewArray(ctx) : list;
    }

    JSValue ageableGetDropItems(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return listGetter(ctx, thisVal, "drop_items");
    }

    JSValue rideableGetFamilyTypes(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return listGetter(ctx, thisVal, "family_types");
    }

    JSValue permutationList(JSContext *ctx, JSValueConst thisVal, const char *key) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        JSValue array = JS_NewArray(ctx);
        const json::Value *blocks = fieldOf(target, key);
        uint32_t index = 0;
        if (blocks != nullptr) {
            for (const std::unique_ptr<json::Value> &block: blocks->mArray) {
                JSValue permutation = target.mApi->makePermutation(block->string());
                if (!JS_IsUndefined(permutation))
                    JS_SetPropertyUint32(ctx, array, index++, permutation);
            }
        }
        return array;
    }

    JSValue breathableGetBreatheBlocks(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return permutationList(ctx, thisVal, "breathe_blocks");
    }

    JSValue breathableGetNonBreatheBlocks(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return permutationList(ctx, thisVal, "non_breathe_blocks");
    }

    double numberField(const json::Value &object, const char *key, double fallback) {
        const json::Value *value = object.get(key);
        return value != nullptr && value->isNumber() ? value->mNumber : fallback;
    }

    // the records the game copies out of the component; their getters read the copy
    struct RecordField {
        const char *mClass;
        const char *mProperty;
        const char *mKey;
        enum class Kind { Number, String, OptionalString, Vector } mKind;
    };

    const RecordField RECORD_FIELDS[] = {
            {"Seat", "lockRiderRotation", "lock_rider_rotation", RecordField::Kind::Number},
            {"Seat", "maxRiderCount", "max_rider_count", RecordField::Kind::Number},
            {"Seat", "minRiderCount", "min_rider_count", RecordField::Kind::Number},
            {"Seat", "position", "position", RecordField::Kind::Vector},
            {"Seat", "seatRotation", "rotate_rider_by", RecordField::Kind::Number},
            {"FeedItem", "healAmount", "heal_amount", RecordField::Kind::Number},
            {"FeedItem", "item", "item", RecordField::Kind::String},
            {"FeedItem", "resultItem", "result_item", RecordField::Kind::OptionalString},
            {"FeedItemEffect", "amplifier", "amplifier", RecordField::Kind::Number},
            {"FeedItemEffect", "chance", "chance", RecordField::Kind::Number},
            {"FeedItemEffect", "duration", "duration", RecordField::Kind::Number},
            {"FeedItemEffect", "name", "name", RecordField::Kind::String},
            {"EntityDefinitionFeedItem", "growth", "growth", RecordField::Kind::Number},
            {"EntityDefinitionFeedItem", "item", "item", RecordField::Kind::String},
            {"EntityDefinitionFeedItem", "resultItem", "result_item", RecordField::Kind::OptionalString},
    };

    JSValue recordGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const ScriptRecordRef *record = ScriptApi::fromContext(ctx)->native<ScriptRecordRef>(thisVal);
        if (record == nullptr)
            return JS_UNDEFINED;
        const RecordField &field = RECORD_FIELDS[magic];
        switch (field.mKind) {
            case RecordField::Kind::Number: {
                const auto it = record->mNumbers.find(field.mKey);
                return JS_NewFloat64(ctx, it == record->mNumbers.end() ? 0.0 : it->second);
            }
            case RecordField::Kind::Vector:
                return vector3(ctx, record->mVector[0], record->mVector[1], record->mVector[2]);
            default: {
                const auto it = record->mStrings.find(field.mKey);
                if (it == record->mStrings.end())
                    return field.mKind == RecordField::Kind::OptionalString ? JS_UNDEFINED : JS_NewString(ctx, "");
                return string(ctx, it->second);
            }
        }
    }

    JSValue feedItemGetEffects(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptRecordRef *record = api->native<ScriptRecordRef>(thisVal);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::shared_ptr<ScriptRecordRef> &child:
             record == nullptr ? std::vector<std::shared_ptr<ScriptRecordRef>>() : record->mChildren)
            JS_SetPropertyUint32(ctx, array, index++, api->makeInstance("FeedItemEffect", new ScriptRecordRef(*child)));
        return array;
    }

    ScriptRecordRef *recordOf(const json::Value &object, const char *const *numbers, const char *const *strings) {
        ScriptRecordRef *record = new ScriptRecordRef();
        for (; *numbers != nullptr; ++numbers) {
            const json::Value *value = object.get(*numbers);
            if (value != nullptr && value->isNumber())
                record->mNumbers[*numbers] = value->mNumber;
        }
        for (; *strings != nullptr; ++strings) {
            const json::Value *value = object.get(*strings);
            if (value != nullptr && value->isString())
                record->mStrings[*strings] = value->mString;
        }
        return record;
    }

    // SeatDescription defaults (sub_14B015000, 26.60): lock_rider_rotation 181, the rest 0
    JSValue rideableGetSeats(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        JSValue array = JS_NewArray(ctx);
        const json::Value *seats = fieldOf(target, "seats");
        if (seats == nullptr)
            return array;
        std::vector<const json::Value *> list;
        if (seats->isArray()) {
            for (const std::unique_ptr<json::Value> &seat: seats->mArray)
                list.push_back(seat.get());
        } else {
            list.push_back(seats);
        }
        static const char *const NUMBERS[] = {"min_rider_count", "max_rider_count", "rotate_rider_by", nullptr};
        static const char *const STRINGS[] = {nullptr};
        uint32_t index = 0;
        for (const json::Value *seat: list) {
            if (!seat->isObject())
                continue;
            ScriptRecordRef *record = recordOf(*seat, NUMBERS, STRINGS);
            record->mNumbers["lock_rider_rotation"] = numberField(*seat, "lock_rider_rotation", 181.0);
            const json::Value *position = seat->get("position");
            if (position != nullptr && position->isArray() && position->mArray.size() >= 3) {
                for (int axis = 0; axis < 3; ++axis)
                    record->mVector[axis] = (float) position->mArray[axis]->number(0.0);
            }
            JS_SetPropertyUint32(ctx, array, index++, target.mApi->makeInstance("Seat", record));
        }
        return array;
    }

    JSValue healableGetFeedItems(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        JSValue array = JS_NewArray(ctx);
        const json::Value *items = fieldOf(target, "items");
        if (items == nullptr || !items->isArray())
            return array;
        static const char *const NUMBERS[] = {"heal_amount", nullptr};
        static const char *const STRINGS[] = {"item", "result_item", nullptr};
        static const char *const EFFECT_NUMBERS[] = {"amplifier", "chance", "duration", nullptr};
        static const char *const EFFECT_STRINGS[] = {"name", nullptr};
        uint32_t index = 0;
        for (const std::unique_ptr<json::Value> &item: items->mArray) {
            if (!item->isObject())
                continue;
            ScriptRecordRef *record = recordOf(*item, NUMBERS, STRINGS);
            const json::Value *effects = item->get("effects");
            if (effects != nullptr && effects->isArray()) {
                for (const std::unique_ptr<json::Value> &effect: effects->mArray) {
                    if (effect->isObject())
                        record->mChildren.emplace_back(recordOf(*effect, EFFECT_NUMBERS, EFFECT_STRINGS));
                }
            }
            JS_SetPropertyUint32(ctx, array, index++, target.mApi->makeInstance("FeedItem", record));
        }
        return array;
    }

    // feed_items: a name or a list of names and {item, growth, result_item}; growth 0.1 when left out, as feeding uses
    JSValue ageableGetFeedItems(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        JSValue array = JS_NewArray(ctx);
        const json::Value *items = fieldOf(target, "feed_items");
        if (items == nullptr)
            return array;
        std::vector<const json::Value *> list;
        if (items->isArray()) {
            for (const std::unique_ptr<json::Value> &item: items->mArray)
                list.push_back(item.get());
        } else {
            list.push_back(items);
        }
        static const char *const NUMBERS[] = {nullptr};
        static const char *const STRINGS[] = {"item", "result_item", nullptr};
        uint32_t index = 0;
        for (const json::Value *item: list) {
            ScriptRecordRef *record = nullptr;
            if (item->isString()) {
                record = new ScriptRecordRef();
                record->mStrings["item"] = item->mString;
                record->mNumbers["growth"] = MobActor::DEFAULT_FEED_GROWTH;
            } else if (item->isObject()) {
                record = recordOf(*item, NUMBERS, STRINGS);
                record->mNumbers["growth"] = numberField(*item, "growth", MobActor::DEFAULT_FEED_GROWTH);
            } else {
                continue;
            }
            JS_SetPropertyUint32(ctx, array, index++, target.mApi->makeInstance("EntityDefinitionFeedItem", record));
        }
        return array;
    }

    // the grow_up event as a Trigger
    JSValue ageableGetGrowUp(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        const json::Value *growUp = fieldOf(target, "grow_up");
        const json::Value *event = growUp == nullptr ? nullptr : growUp->get("event");
        ScriptNamedRef *ref = new ScriptNamedRef();
        ref->mName = event != nullptr && event->isString() ? event->mString : std::string();
        return target.mApi->makeInstance("Trigger", ref);
    }

    JSValue tameableGetTameItems(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        JSValue array = JS_NewArray(ctx);
        const json::Value *items = fieldOf(target, "tame_items");
        uint32_t index = 0;
        const auto add = [&](const json::Value &item) {
            if (item.isString())
                JS_SetPropertyUint32(ctx, array, index++, target.mApi->makeItemStack(item.mString, 1));
        };
        if (items != nullptr && items->isArray()) {
            for (const std::unique_ptr<json::Value> &item: items->mArray)
                add(*item);
        } else if (items != nullptr) {
            add(*items);
        }
        return array;
    }

    // attribute components: the attribute when the entity has one, the definition's value otherwise
    enum class AttributeField { Current, Default, Max, Min };

    JSValue attributeGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        const AttributeData *attribute = attributeOf(*target.mActor, target.mPart->mComponentId);
        if (attribute == nullptr) {
            const json::Value *value = fieldOf(target, "value");
            const json::Value *max = fieldOf(target, "max");
            const double base = value == nullptr ? 0.0 : value->number(0.0);
            if ((AttributeField) magic == AttributeField::Max)
                return JS_NewFloat64(ctx, max == nullptr ? base : max->number(base));
            if ((AttributeField) magic == AttributeField::Min)
                return JS_NewFloat64(ctx, 0.0);
            return JS_NewFloat64(ctx, base);
        }
        switch ((AttributeField) magic) {
            case AttributeField::Current:
                return JS_NewFloat64(ctx, attribute->mValue);
            case AttributeField::Default:
                return JS_NewFloat64(ctx, attribute->mDefaultValue);
            case AttributeField::Max:
                return JS_NewFloat64(ctx, attribute->mMaximum);
            case AttributeField::Min:
                return JS_NewFloat64(ctx, attribute->mMinimum);
        }
        return JS_UNDEFINED;
    }

    void syncAttributes(ScriptApi &api, Actor &actor) {
        if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&actor))
            api.host().syncPlayerAttributes(*player);
        else if (ServerActor *serverActor = dynamic_cast<ServerActor *>(&actor))
            api.host().syncActorAttributes(*serverActor);
    }

    // false when the value is outside the attribute's range
    bool setAttribute(ComponentTarget &target, float value) {
        const AttributeData *attribute = attributeOf(*target.mActor, target.mPart->mComponentId);
        // written so a NaN fails it too
        if (attribute == nullptr || !(value >= attribute->mMinimum && value <= attribute->mMaximum))
            return false;
        target.mActor->getAttributes().set(target.mPart->mComponentId, value);
        syncAttributes(*target.mApi, *target.mActor);
        return true;
    }

    JSValue attributeSetCurrent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        double value = 0.0;
        if (argc < 1 || JS_ToFloat64(ctx, &value, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return JS_NewBool(ctx, setAttribute(target, (float) value));
    }

    JSValue attributeReset(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        const AttributeData *attribute = attributeOf(*target.mActor, target.mPart->mComponentId);
        if (attribute == nullptr)
            return JS_UNDEFINED;
        const float value = (AttributeField) magic == AttributeField::Default ? attribute->mDefaultValue
                            : (AttributeField) magic == AttributeField::Max   ? attribute->mMaximum
                                                                              : attribute->mMinimum;
        setAttribute(target, value);
        return JS_UNDEFINED;
    }

    JSValue typeFamilyHas(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        if (argc < 1)
            return JS_FALSE;
        const std::vector<std::string> families = ScriptApi::entityFamilies(*target.mActor);
        return JS_NewBool(ctx, std::find(families.begin(), families.end(), toString(ctx, argv[0])) != families.end());
    }

    JSValue typeFamilyGetAll(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &family: ScriptApi::entityFamilies(*target.mActor))
            JS_SetPropertyUint32(ctx, array, index++, string(ctx, family));
        return array;
    }

    JSValue componentGetEntity(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        return target.mApi->makeEntity(*target.mActor);
    }

    JSValue onFireGetTicks(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        return JS_NewInt32(ctx, target.mActor->getFireTicks());
    }

    struct AirSupply {
        int32_t mTicks = 0;
        int32_t mMinTicks = 0;
        int32_t mMaxTicks = 0;
    };

    // whole seconds in the definition; absent fields take the schema defaults (sub_143796DC0)
    int32_t definitionSeconds(const ComponentTarget &target, const char *key, int32_t fallback) {
        const json::Value *value = fieldOf(target, key);
        return value != nullptr && value->isNumber() ? (int32_t) value->mNumber : fallback;
    }

    bool airSupplyOf(const ComponentTarget &target, AirSupply &air) {
        air.mMinTicks = definitionSeconds(target, "suffocate_time", -20) * 20;
        air.mMaxTicks = definitionSeconds(target, "total_supply", 15) * 20;
        if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(target.mActor)) {
            air.mTicks = player->getAirSupply();
            return true;
        }
        MobActor *mob = dynamic_cast<MobActor *>(target.mActor);
        if (mob == nullptr)
            return false;
        air.mTicks = mob->getAirSupply();
        return true;
    }

    // whole seconds, truncated (sub_14BDBB230)
    JSValue breathableGetAirSupply(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        AirSupply air;
        if (!airSupplyOf(target, air))
            return target.mApi->throwError("Error", "Failed to call function 'getAirSupply'.");
        return JS_NewInt32(ctx, (int16_t) air.mTicks / 20);
    }

    // seconds within [suffocation, total supply] in whole seconds, stored as ticks (sub_14BDB9D20)
    JSValue breathableSetAirSupply(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        double number = 0.0;
        if (argc < 1 || !JS_IsNumber(argv[0]) || JS_ToFloat64(ctx, &number, argv[0]) != 0 || !std::isfinite(number)
            || number < INT16_MIN || number > INT16_MAX)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        AirSupply air;
        if (!airSupplyOf(target, air))
            return target.mApi->throwError("Error", "Failed to call function 'setAirSupply'.");

        const int16_t seconds = (int16_t) number;
        const int32_t minimum = air.mMinTicks / 20;
        const int32_t maximum = (int16_t) air.mMaxTicks / 20;
        if (seconds < minimum || seconds > maximum) {
            JSValue fields = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, fields, "minValue", JS_NewFloat64(ctx, minimum));
            JS_SetPropertyStr(ctx, fields, "maxValue", JS_NewFloat64(ctx, maximum));
            JS_SetPropertyStr(ctx, fields, "value", JS_NewFloat64(ctx, seconds));
            return target.mApi->throwError("PropertyOutOfBoundsError",
                                           "Unsupported or out of bounds value set on Property: airSupply, Value: "
                                                   + std::to_string(seconds) + ", Property bounds: ["
                                                   + std::to_string(minimum) + ", " + std::to_string(maximum) + "]",
                                           fields);
        }

        const int16_t wanted = (int16_t) (seconds * 20);
        const int32_t ticks = wanted > air.mMaxTicks ? air.mMaxTicks : std::max<int32_t>(wanted, air.mMinTicks);
        if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(target.mActor))
            player->setAirSupply(ticks);
        else
            static_cast<MobActor *>(target.mActor)->setAirSupply(ticks);
        return JS_UNDEFINED;
    }

    JSValue itemGetItemStack(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        ItemActor *item = dynamic_cast<ItemActor *>(target.mActor);
        return item == nullptr ? JS_UNDEFINED : target.mApi->makeItem(item->getItem());
    }

    JSValue ridingGetEntityRidingOn(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        Actor *vehicle = RideSystem::resolve(target.mApi->host(), target.mActor->getVehicleId());
        return vehicle == nullptr ? JS_UNDEFINED : target.mApi->makeEntity(*vehicle);
    }

    JSValue rideableGetRiders(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const int64_t id: target.mActor->getPassengers()) {
            Actor *rider = RideSystem::resolve(target.mApi->host(), id);
            if (rider != nullptr)
                JS_SetPropertyUint32(ctx, array, index++, target.mApi->makeEntity(*rider));
        }
        return array;
    }

    JSValue rideableAddRider(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        ServerActor *vehicle = dynamic_cast<ServerActor *>(target.mActor);
        Actor *rider = argc >= 1 ? target.mApi->resolveEntity(argv[0]) : nullptr;
        if (rider == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return JS_NewBool(ctx, vehicle != nullptr && RideSystem::mount(target.mApi->host(), *rider, *vehicle, false));
    }

    JSValue rideableEjectRider(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        Actor *rider = argc >= 1 ? target.mApi->resolveEntity(argv[0]) : nullptr;
        if (rider == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (rider->getVehicleId() == target.mActor->getUniqueId())
            RideSystem::dismount(target.mApi->host(), *rider, false);
        return JS_UNDEFINED;
    }

    JSValue rideableEjectRiders(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        RideSystem::ejectAll(target.mApi->host(), *target.mActor);
        return JS_UNDEFINED;
    }

    MobActor *mobOf(JSContext *ctx, JSValueConst thisVal, ComponentTarget &target) {
        if (!componentOf(ctx, thisVal, target))
            return nullptr;
        return dynamic_cast<MobActor *>(target.mActor);
    }

    enum class TamedField { IsTamed, TamedToPlayer, TamedToPlayerId, IsTamedToPlayer };

    JSValue tamedGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ComponentTarget target;
        MobActor *mob = mobOf(ctx, thisVal, target);
        if (target.mActor == nullptr)
            return JS_EXCEPTION;
        const bool tamed = mob != nullptr && mob->isTamed();
        ServerPlayer *owner = tamed ? mob->getOwner(target.mApi->host()) : nullptr;
        switch ((TamedField) magic) {
            case TamedField::IsTamed:
                return JS_NewBool(ctx, tamed);
            case TamedField::IsTamedToPlayer:
                return JS_NewBool(ctx, tamed && mob->getOwnerId() != MobActor::NO_OWNER);
            case TamedField::TamedToPlayer:
                return owner == nullptr ? JS_UNDEFINED : target.mApi->makePlayer(*owner);
            case TamedField::TamedToPlayerId:
                if (!tamed || mob->getOwnerId() == MobActor::NO_OWNER)
                    return JS_UNDEFINED;
                return string(ctx, std::to_string(mob->getOwnerId()));
        }
        return JS_UNDEFINED;
    }

    JSValue tameableTame(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        MobActor *mob = mobOf(ctx, thisVal, target);
        if (target.mActor == nullptr)
            return JS_EXCEPTION;
        ServerPlayer *player = argc >= 1 ? target.mApi->resolvePlayer(argv[0]) : nullptr;
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return JS_NewBool(ctx, mob != nullptr && mob->tameTo(target.mApi->host(), *player, "minecraft:tameable", true));
    }

    JSValue tameMountTameToPlayer(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        MobActor *mob = mobOf(ctx, thisVal, target);
        if (target.mActor == nullptr)
            return JS_EXCEPTION;
        const bool particles = argc >= 1 && JS_ToBool(ctx, argv[0]) == 1;
        ServerPlayer *player = argc >= 2 ? target.mApi->resolvePlayer(argv[1]) : nullptr;
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return JS_NewBool(ctx, mob != nullptr
                                       && mob->tameTo(target.mApi->host(), *player, "minecraft:tamemount", particles));
    }

    JSValue tameMountTame(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        MobActor *mob = mobOf(ctx, thisVal, target);
        if (target.mActor == nullptr)
            return JS_EXCEPTION;
        const bool particles = argc >= 1 && JS_ToBool(ctx, argv[0]) == 1;
        if (mob != nullptr)
            mob->tameWithoutOwner(target.mApi->host(), "minecraft:tamemount", particles);
        return JS_UNDEFINED;
    }

    enum class LeashField { IsLeashed, Holder, HolderId };

    JSValue leashGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ComponentTarget target;
        MobActor *mob = mobOf(ctx, thisVal, target);
        if (target.mActor == nullptr)
            return JS_EXCEPTION;
        const bool leashed = mob != nullptr && mob->getLeash().isLeashed();
        if ((LeashField) magic == LeashField::IsLeashed)
            return JS_NewBool(ctx, leashed);
        if (!leashed)
            return JS_UNDEFINED;
        if ((LeashField) magic == LeashField::HolderId)
            return string(ctx, std::to_string(mob->getLeash().getHolderId()));
        Actor *holder = RideSystem::resolve(target.mApi->host(), mob->getLeash().getHolderId());
        return holder == nullptr ? JS_UNDEFINED : target.mApi->makeEntity(*holder);
    }

    JSValue leashTo(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        MobActor *mob = mobOf(ctx, thisVal, target);
        if (target.mActor == nullptr)
            return JS_EXCEPTION;
        Actor *holder = argc >= 1 ? target.mApi->resolveEntity(argv[0]) : nullptr;
        if (holder == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (mob != nullptr)
            mob->getLeash().attach(target.mApi->host(), *mob, holder->getUniqueId());
        return JS_UNDEFINED;
    }

    JSValue leashUnleash(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        MobActor *mob = mobOf(ctx, thisVal, target);
        if (target.mActor == nullptr)
            return JS_EXCEPTION;
        if (mob != nullptr && mob->getLeash().isLeashed())
            mob->getLeash().detach(target.mApi->host(), *mob, true);
        return JS_UNDEFINED;
    }

    JSValue projectileGetOwner(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        ServerActor *actor = dynamic_cast<ServerActor *>(target.mActor);
        if (actor == nullptr)
            return JS_UNDEFINED;
        if (actor->hasOwnerPlayer()) {
            ServerPlayer *owner = target.mApi->resolvePlayerByHandle(actor->getOwnerPlayerHandle());
            return owner == nullptr ? JS_UNDEFINED : target.mApi->makePlayer(*owner);
        }
        ServerActor *owner = actor->getOwnerUniqueId() >= 0 ? target.mApi->host().getActor(actor->getOwnerUniqueId())
                                                            : nullptr;
        return owner == nullptr ? JS_UNDEFINED : target.mApi->makeActor(*owner);
    }

    JSValue projectileSetOwner(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        ServerActor *actor = dynamic_cast<ServerActor *>(target.mActor);
        const ScriptEntityRef *owner = argc >= 1 ? target.mApi->native<ScriptEntityRef>(argv[0]) : nullptr;
        if (actor == nullptr || owner == nullptr)
            return JS_UNDEFINED;
        if (owner->mKind == ScriptEntityRef::Kind::Player)
            actor->setOwnerPlayerHandle((uint32_t) owner->mId);
        else if (ServerActor *resolved = target.mApi->resolveActor(argv[0]))
            actor->setOwnerUniqueId(resolved->getUniqueId());
        return JS_UNDEFINED;
    }

    enum class ProjectileField {
        AirInertia,
        CatchFireOnHurt,
        CritParticles,
        DestroyOnHurt,
        Gravity,
        HitEntitySound,
        HitGroundSound,
        HitParticle,
        Lightning,
        LiquidInertia,
        OnFireTime,
        ShouldBounce,
        StopOnHit,
    };

    const char *const PROJECTILE_FIELDS[] = {"airInertia",
                                             "catchFireOnHurt",
                                             "critParticlesOnProjectileHurt",
                                             "destroyOnProjectileHurt",
                                             "gravity",
                                             "hitEntitySound",
                                             "hitGroundSound",
                                             "hitParticle",
                                             "lightningStrikeOnHit",
                                             "liquidInertia",
                                             "onFireTime",
                                             "shouldBounceOnHit",
                                             "stopOnHit"};

    ProjectileComponent *projectileOf(const ComponentTarget &target) {
        ServerActor *actor = dynamic_cast<ServerActor *>(target.mActor);
        return actor == nullptr ? nullptr : &actor->getProjectileComponent();
    }

    // no sound and no particle read as undefined
    JSValue optionalText(JSContext *ctx, const std::string &text) {
        return text.empty() ? JS_UNDEFINED : string(ctx, text);
    }

    JSValue projectileGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        const ProjectileComponent *projectile = projectileOf(target);
        if (projectile == nullptr)
            return JS_UNDEFINED;
        switch ((ProjectileField) magic) {
            case ProjectileField::AirInertia:
                return JS_NewFloat64(ctx, projectile->mInertia);
            case ProjectileField::CatchFireOnHurt:
                return JS_NewBool(ctx, projectile->mCatchFire);
            case ProjectileField::CritParticles:
                return JS_NewBool(ctx, projectile->mCritParticleOnHurt);
            case ProjectileField::DestroyOnHurt:
                return JS_NewBool(ctx, projectile->mDestroyOnHurt);
            case ProjectileField::Gravity:
                return JS_NewFloat64(ctx, projectile->mGravity);
            case ProjectileField::HitEntitySound:
                return optionalText(ctx, LevelSoundEventIds::nameOf(projectile->mHitSound));
            case ProjectileField::HitGroundSound:
                return optionalText(ctx, LevelSoundEventIds::nameOf(projectile->mHitGroundSound));
            case ProjectileField::HitParticle:
                // ParticleType 0 and a type without a name read as undefined (sub_14C6A7B50, 26.60)
                return optionalText(ctx, ProjectileComponent::particleName(projectile->mParticle));
            case ProjectileField::Lightning:
                return JS_NewBool(ctx, projectile->mLightning);
            case ProjectileField::LiquidInertia:
                return JS_NewFloat64(ctx, projectile->mLiquidInertia);
            case ProjectileField::OnFireTime:
                return JS_NewFloat64(ctx, projectile->mOnFireTime);
            case ProjectileField::ShouldBounce:
                // the getter tests the should_bounce byte (sub_14C6A8770, 26.60)
                return JS_NewBool(ctx, projectile->mShouldBounce != ProjectileComponent::Bounce::No);
            case ProjectileField::StopOnHit:
                return JS_NewBool(ctx, projectile->mStopOnHurt);
        }
        return JS_UNDEFINED;
    }

    JSValue projectileSet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        ProjectileComponent *projectile = projectileOf(target);
        const JSValueConst value = argc >= 1 ? argv[0] : JS_UNDEFINED;
        const ProjectileField field = (ProjectileField) magic;
        const bool text = field == ProjectileField::HitEntitySound || field == ProjectileField::HitGroundSound
                          || field == ProjectileField::HitParticle;
        const bool number = field == ProjectileField::AirInertia || field == ProjectileField::Gravity
                            || field == ProjectileField::LiquidInertia || field == ProjectileField::OnFireTime;
        if (text ? !JS_IsString(value) && !JS_IsUndefined(value) : number ? !JS_IsNumber(value) : !JS_IsBool(value))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (projectile == nullptr)
            return JS_UNDEFINED;

        double amount = 0.0;
        if (number)
            JS_ToFloat64(ctx, &amount, value);
        const bool flag = JS_ToBool(ctx, value) == 1;
        const std::string name = JS_IsString(value) ? toString(ctx, value) : std::string();
        switch (field) {
            case ProjectileField::AirInertia:
                projectile->mInertia = (float) amount;
                break;
            case ProjectileField::CatchFireOnHurt:
                projectile->mCatchFire = flag;
                break;
            case ProjectileField::CritParticles:
                projectile->mCritParticleOnHurt = flag;
                break;
            case ProjectileField::DestroyOnHurt:
                projectile->mDestroyOnHurt = flag;
                break;
            case ProjectileField::Gravity:
                projectile->mGravity = (float) amount;
                break;
            case ProjectileField::HitEntitySound:
            case ProjectileField::HitGroundSound: {
                // undefined clears the sound; a name that maps to Undefined throws, "undefined" too (sub_14C6A5960)
                const int32_t sound = JS_IsUndefined(value) ? LevelSoundEventIds::UNDEFINED
                                                            : LevelSoundEventIds::fromName(name);
                if (sound == LevelSoundEventIds::UNDEFINED && !JS_IsUndefined(value))
                    return target.mApi->throwError("InvalidArgumentError", "Invalid sound identifier.");
                (field == ProjectileField::HitEntitySound ? projectile->mHitSound
                                                          : projectile->mHitGroundSound) = sound;
                break;
            }
            case ProjectileField::HitParticle: {
                // undefined clears the particle, a name without a type throws (sub_14C6A8150, 26.60)
                const int32_t type = JS_IsUndefined(value) ? 0 : ProjectileComponent::particleType(name);
                if (type == 0 && !JS_IsUndefined(value))
                    return target.mApi->throwError("InvalidArgumentError", "Invalid particle identifier.");
                projectile->mParticle = type;
                break;
            }
            case ProjectileField::Lightning:
                projectile->mLightning = flag;
                break;
            case ProjectileField::LiquidInertia:
                projectile->mLiquidInertia = (float) amount;
                break;
            case ProjectileField::OnFireTime:
                projectile->mOnFireTime = (float) amount;
                break;
            case ProjectileField::ShouldBounce:
                // the setter stores the boolean in the should_bounce byte (sub_14C6A8A30, 26.60)
                projectile->mShouldBounce = (ProjectileComponent::Bounce)(flag ? 1 : 0);
                break;
            case ProjectileField::StopOnHit:
                projectile->mStopOnHurt = flag;
                break;
        }
        return JS_UNDEFINED;
    }

    JSValue projectileShoot(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ComponentTarget target;
        if (!componentOf(ctx, thisVal, target))
            return JS_EXCEPTION;
        ServerActor *actor = dynamic_cast<ServerActor *>(target.mActor);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (actor != nullptr) {
            actor->setMotion(Vector3f((float) x, (float) y, (float) z));
            actor->setProjectile(true);
            target.mApi->host().sendActorMotion(*actor);
        }
        return JS_UNDEFINED;
    }
}

namespace {
    enum class NpcField { Name, DefaultScene, SkinIndex };

    NpcActor *npcOf(JSContext *ctx, JSValueConst thisVal, ComponentTarget &target) {
        if (!componentOf(ctx, thisVal, target))
            return nullptr;
        NpcActor *npc = dynamic_cast<NpcActor *>(target.mActor);
        if (npc == nullptr)
            target.mApi->throwError("InvalidEntityError", "Failed to get the component: the entity is invalid.");
        return npc;
    }

    // ScriptNpcComponent getters (sub_14DEA0400, sub_14DEE1C50, 26.60): the raw text name, the scene, the skin
    JSValue npcGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ComponentTarget target;
        NpcActor *npc = npcOf(ctx, thisVal, target);
        if (npc == nullptr)
            return JS_EXCEPTION;
        switch ((NpcField) magic) {
            case NpcField::Name:
                return string(ctx, npc->getRawName());
            case NpcField::DefaultScene:
                return string(ctx, npc->getSceneName());
            case NpcField::SkinIndex:
                return JS_NewInt32(ctx, npc->getSkinIndex());
        }
        return JS_UNDEFINED;
    }

    JSValue npcSet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ComponentTarget target;
        NpcActor *npc = npcOf(ctx, thisVal, target);
        if (npc == nullptr)
            return JS_EXCEPTION;
        const NpcField field = (NpcField) magic;
        if (field == NpcField::SkinIndex) {
            int32_t index = 0;
            if (argc < 1 || !JS_IsNumber(argv[0]) || JS_ToInt32(ctx, &index, argv[0]) != 0)
                return JS_ThrowTypeError(ctx, "Native type conversion failed.");
            if (npc->setSkinIndex(&target.mApi->host(), index))
                return JS_UNDEFINED;
            // sub_14DEDFE30 (26.60): any refused index, the current one too, throws with the list bounds
            const int32_t maxValue = (int32_t) npc->getSkins().size() - 1;
            JSValue fields = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, fields, "value", JS_NewInt32(ctx, index));
            JS_SetPropertyStr(ctx, fields, "minValue", JS_NewInt32(ctx, 0));
            JS_SetPropertyStr(ctx, fields, "maxValue", JS_NewInt32(ctx, maxValue));
            return target.mApi->throwError("ArgumentOutOfBoundsError",
                                           "Unsupported or out of bounds value passed to function argument [0]: "
                                           "Invalid skin index, Value: "
                                                   + std::to_string(index) + ", Argument bounds: [0, "
                                                   + std::to_string(maxValue) + "]",
                                           fields);
        }
        if (argc < 1 || !JS_IsString(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::string text = toString(ctx, argv[0]);
        if (field == NpcField::Name)
            npc->setName(&target.mApi->host(), text);
        else
            npc->setSceneName(text);
        return JS_UNDEFINED;
    }
}

JSValue ScriptApi::makeEntityComponent(Actor &actor, const ScriptEntityRef &ref, const std::string &requested) {
    const std::string id = requested.find(':') == std::string::npos ? "minecraft:" + requested : requested;
    const ComponentClass *component = findComponent(id);
    if (component == nullptr || !mSurface.has(component->mClass) || !hasComponent(actor, *component))
        return JS_UNDEFINED;

    ScriptEntityPart *part = new ScriptEntityPart();
    part->mEntity = ref;
    part->mComponentId = id;
    return makeInstance(component->mClass, part);
}

std::vector<std::string> ScriptApi::entityComponentIds(Actor &actor) {
    std::vector<std::string> ids;
    for (const ComponentClass &component: COMPONENTS) {
        if (mSurface.has(component.mClass) && hasComponent(actor, component))
            ids.emplace_back(component.mId);
    }
    return ids;
}

void ScriptApi::_bindEntityComponents() {
    JSValue base = defineClass("EntityComponent");
    property(base, "EntityComponent", "entity", componentGetEntity);

    JSValue attribute = defineClass("EntityAttributeComponent");
    propertyMagic(attribute, "EntityAttributeComponent", "currentValue", attributeGet, (int) AttributeField::Current);
    propertyMagic(attribute, "EntityAttributeComponent", "defaultValue", attributeGet, (int) AttributeField::Default);
    propertyMagic(attribute, "EntityAttributeComponent", "effectiveMax", attributeGet, (int) AttributeField::Max);
    propertyMagic(attribute, "EntityAttributeComponent", "effectiveMin", attributeGet, (int) AttributeField::Min);
    method(attribute, "EntityAttributeComponent", "setCurrentValue", attributeSetCurrent, 1);
    methodMagic(attribute, "EntityAttributeComponent", "resetToDefaultValue", attributeReset, 0,
                (int) AttributeField::Default);
    methodMagic(attribute, "EntityAttributeComponent", "resetToMaxValue", attributeReset, 0, (int) AttributeField::Max);
    methodMagic(attribute, "EntityAttributeComponent", "resetToMinValue", attributeReset, 0, (int) AttributeField::Min);

    // the 1.x typings declare componentId before EntityComponentTypes lists the component
    for (const ComponentClass &component: COMPONENTS) {
        defineClass(component.mClass);
        if (mSurface.has(std::string(component.mClass) + ".static componentId"))
            JS_DefinePropertyValueStr(mContext, constructorOf(component.mClass), "componentId",
                                      string(mContext, component.mId), JS_PROP_ENUMERABLE);
    }

    for (size_t i = 0; i < sizeof(FIELDS) / sizeof(FIELDS[0]); ++i) {
        const DefinitionField &field = FIELDS[i];
        propertyMagic(prototypeOf(field.mClass), field.mClass, field.mProperty, fieldGetter, (int) i);
    }
    property(prototypeOf("EntityVariantComponent"), "EntityVariantComponent", "value", variantGetValue);

    method(prototypeOf("EntityAgeableComponent"), "EntityAgeableComponent", "getDropItems", ageableGetDropItems, 0);
    method(prototypeOf("EntityBreathableComponent"), "EntityBreathableComponent", "getBreatheBlocks",
           breathableGetBreatheBlocks, 0);
    method(prototypeOf("EntityBreathableComponent"), "EntityBreathableComponent", "getNonBreatheBlocks",
           breathableGetNonBreatheBlocks, 0);
    property(prototypeOf("EntityBreathableComponent"), "EntityBreathableComponent", "airSupply",
             breathableGetAirSupply, breathableSetAirSupply);

    JSValue family = prototypeOf("EntityTypeFamilyComponent");
    method(family, "EntityTypeFamilyComponent", "hasTypeFamily", typeFamilyHas, 1);
    method(family, "EntityTypeFamilyComponent", "getTypeFamilies", typeFamilyGetAll, 0);

    property(prototypeOf("EntityOnFireComponent"), "EntityOnFireComponent", "onFireTicksRemaining", onFireGetTicks);
    property(prototypeOf("EntityItemComponent"), "EntityItemComponent", "itemStack", itemGetItemStack);
    property(prototypeOf("EntityRidingComponent"), "EntityRidingComponent", "entityRidingOn", ridingGetEntityRidingOn);

    JSValue rideable = prototypeOf("EntityRideableComponent");
    method(rideable, "EntityRideableComponent", "getFamilyTypes", rideableGetFamilyTypes, 0);
    method(rideable, "EntityRideableComponent", "getSeats", rideableGetSeats, 0);
    method(prototypeOf("EntityHealableComponent"), "EntityHealableComponent", "getFeedItems", healableGetFeedItems, 0);
    method(prototypeOf("EntityAgeableComponent"), "EntityAgeableComponent", "getFeedItems", ageableGetFeedItems, 0);
    property(prototypeOf("EntityAgeableComponent"), "EntityAgeableComponent", "growUp", ageableGetGrowUp);
    for (const char *record: {"Seat", "FeedItem", "FeedItemEffect", "EntityDefinitionFeedItem"}) {
        if (mSurface.has(record))
            defineClass(record);
    }
    for (size_t i = 0; i < sizeof(RECORD_FIELDS) / sizeof(RECORD_FIELDS[0]); ++i) {
        const RecordField &field = RECORD_FIELDS[i];
        if (!JS_IsUndefined(prototypeOf(field.mClass)))
            propertyMagic(prototypeOf(field.mClass), field.mClass, field.mProperty, recordGet, (int) i);
    }
    if (!JS_IsUndefined(prototypeOf("FeedItem")))
        method(prototypeOf("FeedItem"), "FeedItem", "getEffects", feedItemGetEffects, 0);
    method(rideable, "EntityRideableComponent", "getRiders", rideableGetRiders, 0);
    method(rideable, "EntityRideableComponent", "addRider", rideableAddRider, 1);
    method(rideable, "EntityRideableComponent", "ejectRider", rideableEjectRider, 1);
    method(rideable, "EntityRideableComponent", "ejectRiders", rideableEjectRiders, 0);

    JSValue isTamed = prototypeOf("EntityIsTamedComponent");
    propertyMagic(isTamed, "EntityIsTamedComponent", "tamedToPlayer", tamedGet, (int) TamedField::TamedToPlayer);
    propertyMagic(isTamed, "EntityIsTamedComponent", "tamedToPlayerId", tamedGet, (int) TamedField::TamedToPlayerId);

    JSValue tameable = prototypeOf("EntityTameableComponent");
    propertyMagic(tameable, "EntityTameableComponent", "isTamed", tamedGet, (int) TamedField::IsTamed);
    propertyMagic(tameable, "EntityTameableComponent", "tamedToPlayer", tamedGet, (int) TamedField::TamedToPlayer);
    propertyMagic(tameable, "EntityTameableComponent", "tamedToPlayerId", tamedGet, (int) TamedField::TamedToPlayerId);
    method(tameable, "EntityTameableComponent", "getTameItems", tameableGetTameItems, 0);
    method(tameable, "EntityTameableComponent", "tame", tameableTame, 1);

    JSValue tameMount = prototypeOf("EntityTameMountComponent");
    propertyMagic(tameMount, "EntityTameMountComponent", "isTamed", tamedGet, (int) TamedField::IsTamed);
    propertyMagic(tameMount, "EntityTameMountComponent", "isTamedToPlayer", tamedGet,
                  (int) TamedField::IsTamedToPlayer);
    propertyMagic(tameMount, "EntityTameMountComponent", "tamedToPlayer", tamedGet, (int) TamedField::TamedToPlayer);
    propertyMagic(tameMount, "EntityTameMountComponent", "tamedToPlayerId", tamedGet,
                  (int) TamedField::TamedToPlayerId);
    method(tameMount, "EntityTameMountComponent", "tame", tameMountTame, 1);
    method(tameMount, "EntityTameMountComponent", "tameToPlayer", tameMountTameToPlayer, 2);

    JSValue leashable = prototypeOf("EntityLeashableComponent");
    propertyMagic(leashable, "EntityLeashableComponent", "isLeashed", leashGet, (int) LeashField::IsLeashed);
    propertyMagic(leashable, "EntityLeashableComponent", "leashHolder", leashGet, (int) LeashField::Holder);
    propertyMagic(leashable, "EntityLeashableComponent", "leashHolderEntityId", leashGet, (int) LeashField::HolderId);
    method(leashable, "EntityLeashableComponent", "leashTo", leashTo, 1);
    method(leashable, "EntityLeashableComponent", "unleash", leashUnleash, 0);

    JSValue projectile = prototypeOf("EntityProjectileComponent");
    property(projectile, "EntityProjectileComponent", "owner", projectileGetOwner, projectileSetOwner);
    for (size_t i = 0; i < sizeof(PROJECTILE_FIELDS) / sizeof(PROJECTILE_FIELDS[0]); ++i)
        propertyMagic(projectile, "EntityProjectileComponent", PROJECTILE_FIELDS[i], projectileGet, (int) i,
                      projectileSet);
    method(projectile, "EntityProjectileComponent", "shoot", projectileShoot, 2);

    JSValue npc = prototypeOf("EntityNpcComponent");
    propertyMagic(npc, "EntityNpcComponent", "name", npcGet, (int) NpcField::Name, npcSet);
    propertyMagic(npc, "EntityNpcComponent", "defaultScene", npcGet, (int) NpcField::DefaultScene, npcSet);
    propertyMagic(npc, "EntityNpcComponent", "skinIndex", npcGet, (int) NpcField::SkinIndex, npcSet);
}
