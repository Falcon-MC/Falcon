#include "Scripting/Binding/ScriptApi.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ActorDamageSource.h"
#include "Scripting/Binding/ScriptRaycast.h"
#include "Scripting/Binding/ScriptRayQueries.h"
#include "Protocol/Packets/ScriptMessagePacket.h"
#include "Protocol/Packets/StopSoundPacket.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/VanillaActorTable.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockTags.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/VanillaTexts.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/NetworkHandler.h"

#include "Protocol/Packets/CameraInstructionPacket.h"
#include "Protocol/Packets/CameraShakePacket.h"
#include "Protocol/Types/CameraPresets.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

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
        return ScriptApi::entityFamilies(*target.mActor);
    }

    const char *gameModeName(const EntityTarget &target) {
        return target.mApi->gameModeName(target.mPlayer->getGameType());
    }

    bool readNumber(JSContext *ctx, JSValueConst object, const char *key, double &out) {
        JSValue value = JS_GetPropertyStr(ctx, object, key);
        const bool present = !JS_IsUndefined(value) && JS_ToFloat64(ctx, &out, value) == 0;
        JS_FreeValue(ctx, value);
        return present;
    }

    int32_t playerLevelOf(const EntityTarget &target) {
        return target.mPlayer == nullptr ? 0 : target.mPlayer->getExperience().getXpLevel();
    }

    // vertical rotation is the pitch (x), horizontal the yaw (y), both in degrees
    bool rotationMatches(JSContext *ctx, const EntityTarget &target, JSValueConst options) {
        const Vector3f &rotation = target.mActor->getRotation();
        double bound = 0.0;
        if (readNumber(ctx, options, "minVerticalRotation", bound) && rotation.x < bound)
            return false;
        if (readNumber(ctx, options, "maxVerticalRotation", bound) && rotation.x > bound)
            return false;
        if (readNumber(ctx, options, "minHorizontalRotation", bound) && rotation.y < bound)
            return false;
        if (readNumber(ctx, options, "maxHorizontalRotation", bound) && rotation.y > bound)
            return false;
        return true;
    }

    // a value or an EqualsComparison-style object: equals, notEquals, greaterThan(OrEquals), lessThan(OrEquals),
    // lowerBound/upperBound
    bool compareProperty(JSContext *ctx, JSValueConst wanted, JSValueConst actual) {
        if (!JS_IsObject(wanted)) {
            if (JS_IsBool(wanted) || JS_IsString(wanted))
                return toString(ctx, wanted) == toString(ctx, actual);
            double expected = 0.0, value = 0.0;
            return JS_ToFloat64(ctx, &expected, wanted) == 0 && JS_ToFloat64(ctx, &value, actual) == 0
                   && expected == value;
        }

        double value = 0.0;
        const bool numeric = JS_IsNumber(actual) && JS_ToFloat64(ctx, &value, actual) == 0;
        JSValue equals = JS_GetPropertyStr(ctx, wanted, "equals");
        JSValue notEquals = JS_GetPropertyStr(ctx, wanted, "notEquals");
        bool matches = true;
        if (!JS_IsUndefined(equals))
            matches = compareProperty(ctx, equals, actual);
        if (matches && !JS_IsUndefined(notEquals))
            matches = !compareProperty(ctx, notEquals, actual);
        JS_FreeValue(ctx, equals);
        JS_FreeValue(ctx, notEquals);

        double bound = 0.0;
        if (matches && readNumber(ctx, wanted, "greaterThan", bound))
            matches = numeric && value > bound;
        if (matches && readNumber(ctx, wanted, "greaterThanOrEquals", bound))
            matches = numeric && value >= bound;
        if (matches && readNumber(ctx, wanted, "lessThan", bound))
            matches = numeric && value < bound;
        if (matches && readNumber(ctx, wanted, "lessThanOrEquals", bound))
            matches = numeric && value <= bound;
        if (matches && readNumber(ctx, wanted, "lowerBound", bound))
            matches = numeric && value >= bound;
        if (matches && readNumber(ctx, wanted, "upperBound", bound))
            matches = numeric && value <= bound;
        return matches;
    }

    JSValue propertyValue(JSContext *ctx, ServerActor &actor, const ActorPropertyDescription &descriptor);

    bool propertiesMatch(JSContext *ctx, const EntityTarget &target, JSValueConst options) {
        JSValue list = JS_GetPropertyStr(ctx, options, "propertyOptions");
        bool matches = true;
        if (JS_IsArray(list)) {
            uint32_t length = 0;
            JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
            JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);
            for (uint32_t i = 0; i < length && matches; ++i) {
                JSValue entry = JS_GetPropertyUint32(ctx, list, i);
                JSValue idValue = JS_GetPropertyStr(ctx, entry, "propertyId");
                JSValue excludeValue = JS_GetPropertyStr(ctx, entry, "exclude");
                JSValue wanted = JS_GetPropertyStr(ctx, entry, "value");
                const std::string id = toString(ctx, idValue);
                const bool exclude = JS_ToBool(ctx, excludeValue) == 1;

                const ActorPropertyDescription *descriptor = target.mServerActor == nullptr
                                                                     ? nullptr
                                                                     : target.mServerActor->findPropertyDescription(id);
                bool found = descriptor != nullptr;
                if (found && !JS_IsUndefined(wanted)) {
                    JSValue actual = propertyValue(ctx, *target.mServerActor, *descriptor);
                    found = compareProperty(ctx, wanted, actual);
                    JS_FreeValue(ctx, actual);
                }
                matches = exclude ? !found : found;
                JS_FreeValue(ctx, wanted);
                JS_FreeValue(ctx, excludeValue);
                JS_FreeValue(ctx, idValue);
                JS_FreeValue(ctx, entry);
            }
        }
        JS_FreeValue(ctx, list);
        return matches;
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

        const std::string name = target.mActor->getName();
        JSValue nameValue = JS_GetPropertyStr(ctx, options, "name");
        const bool nameMismatch = JS_IsString(nameValue) && toString(ctx, nameValue) != name;
        JS_FreeValue(ctx, nameValue);
        JSValue excludeNames = JS_GetPropertyStr(ctx, options, "excludeNames");
        const bool nameExcluded = arrayContainsString(ctx, excludeNames, name);
        JS_FreeValue(ctx, excludeNames);
        if (nameMismatch || nameExcluded)
            return false;

        JSValue gameMode = JS_GetPropertyStr(ctx, options, "gameMode");
        JSValue excludeGameModes = JS_GetPropertyStr(ctx, options, "excludeGameModes");
        bool gameModeMismatch = false;
        // only players have a game mode: any game mode filter leaves other entities out
        if (target.mPlayer == nullptr) {
            gameModeMismatch = JS_IsString(gameMode);
        } else {
            const std::string mode = gameModeName(target);
            gameModeMismatch = (JS_IsString(gameMode) && toString(ctx, gameMode) != mode)
                               || arrayContainsString(ctx, excludeGameModes, mode);
        }
        JS_FreeValue(ctx, gameMode);
        JS_FreeValue(ctx, excludeGameModes);
        if (gameModeMismatch)
            return false;

        // levels are experience levels: other entities have none and fail any bound
        double level = 0.0;
        const bool hasMin = readNumber(ctx, options, "minLevel", level);
        if (hasMin && (target.mPlayer == nullptr || playerLevelOf(target) < level))
            return false;
        const bool hasMax = readNumber(ctx, options, "maxLevel", level);
        if (hasMax && (target.mPlayer == nullptr || playerLevelOf(target) > level))
            return false;

        return rotationMatches(ctx, target, options) && propertiesMatch(ctx, target, options)
               && target.mApi->scoresMatch(*target.mActor, options);
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

    enum class EntityState { Sneaking, Sprinting, Climbing, Falling, InWater, OnGround, Sleeping, Swimming };

    const Block *blockAt(Level &level, const Vector3f &position) {
        const BlockState *state = level.peekBlockPtr((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                                                     (int32_t) std::floor(position.z));
        return state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
    }

    JSValue entityGetFlag(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const EntityTarget target = entityOf(ctx, thisVal);
        Actor &actor = *target.mActor;
        switch ((EntityState) magic) {
            case EntityState::Sneaking:
                return JS_NewBool(ctx, actor.getFlags().get(ActorFlag::Sneaking));
            case EntityState::Sprinting:
                return JS_NewBool(ctx, actor.getFlags().get(ActorFlag::Sprinting));
            case EntityState::Climbing: {
                const Block *block = blockAt(target.mApi->host().getLevelFor(actor), actor.getPosition());
                return JS_NewBool(ctx, block != nullptr && block->isClimbable());
            }
            case EntityState::Falling:
                return JS_NewBool(ctx, !actor.isOnGround() && actor.getFallDistance() > 0.0f);
            case EntityState::InWater:
                return JS_NewBool(
                        ctx, LiquidBlocksFetch::at(target.mApi->host().getLevelFor(actor), actor.getPosition()).water);
            case EntityState::OnGround:
                return JS_NewBool(ctx, actor.isOnGround());
            case EntityState::Sleeping:
                return JS_NewBool(ctx, target.mPlayer != nullptr ? target.mPlayer->isSleeping()
                                                                 : actor.getFlags().get(ActorFlag::Sleeping));
            case EntityState::Swimming:
                return JS_NewBool(ctx, actor.getFlags().get(ActorFlag::Swimming));
        }
        return JS_FALSE;
    }

    JSValue entitySetSneaking(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        const bool sneaking = argc >= 1 && JS_ToBool(ctx, argv[0]) == 1;
        if (target.mActor->getFlags().get(ActorFlag::Sneaking) == sneaking)
            return JS_UNDEFINED;

        target.mActor->getFlags().set(ActorFlag::Sneaking, sneaking);
        if (target.mPlayer != nullptr)
            target.mApi->host()._sendEntityData(*target.mPlayer);
        else if (target.mServerActor != nullptr)
            target.mApi->host().syncActorFlags(*target.mServerActor);
        return JS_UNDEFINED;
    }

    // entity.<name>.name, the vanilla namespace dropped (sub_14C0E56A0)
    std::string localizationKeyOf(const std::string &typeId) {
        const std::string prefix = "minecraft:";
        const std::string name = typeId.compare(0, prefix.size(), prefix) == 0 ? typeId.substr(prefix.size()) : typeId;
        return "entity." + name + ".name";
    }

    JSValue entityGetLocalizationKey(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, localizationKeyOf(entityOf(ctx, thisVal).mRef->mTypeId));
    }

    JSValue entityGetTarget(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        MobActor *mob = dynamic_cast<MobActor *>(target.mActor);
        Actor *targeted = mob == nullptr ? nullptr : mob->getTarget(target.mApi->host());
        return targeted == nullptr ? JS_UNDEFINED : target.mApi->makeEntity(*targeted);
    }

    ActorSize sizeOf(const EntityTarget &target) {
        if (target.mPlayer != nullptr) {
            const ActorFlags &flags = target.mPlayer->getFlags();
            float height = MovementPhysics::PLAYER_HEIGHT;
            if (flags.get(ActorFlag::Swimming) || flags.get(ActorFlag::Gliding) || flags.get(ActorFlag::Crawling))
                height = MovementPhysics::PLAYER_CRAWLING_HEIGHT;
            else if (flags.get(ActorFlag::Sneaking))
                height = MovementPhysics::PLAYER_SNEAKING_HEIGHT;
            return ActorSize{MovementPhysics::PLAYER_WIDTH, height};
        }
        if (target.mServerActor != nullptr)
            return target.mServerActor->getSize();
        // the item entity's setSize(0.25, 0.25): sub_1438CA890
        return ActorSize{0.25f, 0.25f};
    }

    // AABB: the center and the half extents of the collision box
    JSValue entityGetAABB(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        const ActorSize size = sizeOf(target);
        const Vector3f &position = target.mActor->getPosition();
        JSValue box = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, box, "center", vector3(ctx, position.x, position.y + size.mHeight / 2.0f, position.z));
        JS_SetPropertyStr(ctx, box, "extent",
                          vector3(ctx, size.mWidth / 2.0f, size.mHeight / 2.0f, size.mWidth / 2.0f));
        return box;
    }

    bool stateMatches(JSContext *ctx, const BlockState &state, JSValueConst permutation) {
        JSValue type = JS_GetPropertyStr(ctx, permutation, "type");
        JSValue id = JS_IsObject(type) ? JS_GetPropertyStr(ctx, type, "id") : JS_UNDEFINED;
        const bool sameType = toString(ctx, id) == state.mName;
        JS_FreeValue(ctx, id);
        JS_FreeValue(ctx, type);
        if (!sameType)
            return false;

        JSAtom atom = JS_NewAtom(ctx, "getAllStates");
        JSValue states = JS_Invoke(ctx, permutation, atom, 0, nullptr);
        JS_FreeAtom(ctx, atom);
        bool matches = JS_IsObject(states);
        JSPropertyEnum *properties = nullptr;
        uint32_t count = 0;
        if (matches && JS_GetOwnPropertyNames(ctx, &properties, &count, states, JS_GPN_STRING_MASK) == 0) {
            for (uint32_t i = 0; i < count && matches; ++i) {
                JSValue key = JS_AtomToString(ctx, properties[i].atom);
                JSValue wanted = JS_GetProperty(ctx, states, properties[i].atom);
                const Tag *actual = state.mStates.isCompound() ? state.mStates.get(toString(ctx, key)) : nullptr;
                if (actual == nullptr)
                    matches = false;
                else if (actual->getType() == Tag::Type::String)
                    matches = toString(ctx, wanted) == actual->asString();
                else if (actual->getType() == Tag::Type::Byte)
                    matches = (JS_ToBool(ctx, wanted) == 1) == (actual->asByte() != 0);
                else {
                    int32_t number = 0;
                    JS_ToInt32(ctx, &number, wanted);
                    matches = number == (actual->getType() == Tag::Type::Short ? actual->asShort() : actual->asInt());
                }
                JS_FreeValue(ctx, wanted);
                JS_FreeValue(ctx, key);
            }
            JS_FreePropertyEnum(ctx, properties, count);
        }
        JS_FreeValue(ctx, states);
        return matches;
    }

    // one entry of the list matches: a type, a tag or a permutation
    bool listMatches(JSContext *ctx, const BlockState &state, JSValueConst list, int kind) {
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        bool found = false;
        for (uint32_t i = 0; i < length && !found; ++i) {
            JSValue entry = JS_GetPropertyUint32(ctx, list, i);
            if (kind == 0) {
                std::string name = toString(ctx, entry);
                if (name.find(':') == std::string::npos)
                    name = "minecraft:" + name;
                found = name == state.mName;
            } else if (kind == 1) {
                found = BlockTags::has(state, toString(ctx, entry));
            } else {
                found = stateMatches(ctx, state, entry);
            }
            JS_FreeValue(ctx, entry);
        }
        return found;
    }

    // BlockFilter: any exclude list rejects; with include lists, one entry of one of them must match
    bool blockStatePassesFilter(JSContext *ctx, const BlockState &state, JSValueConst filter) {
        if (!JS_IsObject(filter))
            return true;
        static const char *const EXCLUDES[] = {"excludeTypes", "excludeTags", "excludePermutations"};
        static const char *const INCLUDES[] = {"includeTypes", "includeTags", "includePermutations"};
        bool rejected = false, anyInclude = false, included = false;
        for (int kind = 0; kind < 3; ++kind) {
            JSValue exclude = JS_GetPropertyStr(ctx, filter, EXCLUDES[kind]);
            if (!rejected && JS_IsArray(exclude))
                rejected = listMatches(ctx, state, exclude, kind);
            JS_FreeValue(ctx, exclude);
            JSValue include = JS_GetPropertyStr(ctx, filter, INCLUDES[kind]);
            if (JS_IsArray(include)) {
                anyInclude = true;
                included = included || listMatches(ctx, state, include, kind);
            }
            JS_FreeValue(ctx, include);
        }
        return !rejected && (!anyInclude || included);
    }


    // the non-air blocks right under the collision box footprint
    std::vector<Vector3i> blocksStandingOn(JSContext *ctx, const EntityTarget &target, JSValueConst options) {
        std::vector<Vector3i> blocks;
        if (!target.mActor->isOnGround())
            return blocks;

        JSValue filter = JS_IsObject(options) ? JS_GetPropertyStr(ctx, options, "blockFilter") : JS_UNDEFINED;
        Level &level = target.mApi->host().getLevelFor(*target.mActor);
        const ActorSize size = sizeOf(target);
        const Vector3f &position = target.mActor->getPosition();
        const float half = size.mWidth / 2.0f;
        const int32_t y = (int32_t) std::floor(position.y - 0.01f);
        for (int32_t x = (int32_t) std::floor(position.x - half); x <= (int32_t) std::floor(position.x + half); ++x) {
            for (int32_t z = (int32_t) std::floor(position.z - half); z <= (int32_t) std::floor(position.z + half);
                 ++z) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr || state->mName == "minecraft:air" || !blockStatePassesFilter(ctx, *state, filter))
                    continue;
                blocks.emplace_back(x, y, z);
            }
        }
        JS_FreeValue(ctx, filter);
        return blocks;
    }

    JSValue entityGetBlockStandingOn(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        const std::vector<Vector3i> blocks = blocksStandingOn(ctx, target, argc >= 1 ? argv[0] : JS_UNDEFINED);
        if (blocks.empty())
            return JS_UNDEFINED;

        // the block under the feet when it is one of them, else the first
        const Vector3f &position = target.mActor->getPosition();
        Vector3i chosen = blocks.front();
        for (const Vector3i &block: blocks) {
            if (block.x == (int32_t) std::floor(position.x) && block.z == (int32_t) std::floor(position.z))
                chosen = block;
        }
        return target.mApi->makeBlock(target.mApi->host().getLevelFor(*target.mActor), chosen.x, chosen.y, chosen.z);
    }

    JSValue entityGetAllBlocksStandingOn(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        Level &level = target.mApi->host().getLevelFor(*target.mActor);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const Vector3i &block: blocksStandingOn(ctx, target, argc >= 1 ? argv[0] : JS_UNDEFINED))
            JS_SetPropertyUint32(ctx, array, index++, target.mApi->makeBlock(level, block.x, block.y, block.z));
        return array;
    }

    // relative head offsets: up times the box height, forward times the box width (sub_14297BAA0 callers)
    struct HeadOffset {
        const char *mIdentifier;
        float mForward;
        float mUp;
    };

    const HeadOffset HEAD_OFFSETS[] = {
            {"minecraft:armadillo", 0.6f, 0.84f},
            {"minecraft:axolotl", 0.85f, 0.85f},
            {"minecraft:bee", 0.85f, 0.85f},
            {"minecraft:breeze", 0.0f, 0.7f},
            {"minecraft:camel", 0.85f, 0.85f},
            {"minecraft:camel_husk", 0.85f, 0.85f},
            {"minecraft:cat", 0.85f, 0.85f},
            {"minecraft:chicken", 0.85f, 0.85f},
            {"minecraft:cow", 0.85f, 0.85f},
            {"minecraft:ender_dragon", -0.4375f, 0.25f},
            {"minecraft:ghast", 0.0f, 0.5f},
            {"minecraft:goat", 0.85f, 0.85f},
            {"minecraft:happy_ghast", 0.0f, 0.5f},
            {"minecraft:horse", 0.85f, 0.85f},
            {"minecraft:llama", 0.85f, 0.85f},
            {"minecraft:mooshroom", 0.85f, 0.85f},
            {"minecraft:nautilus", -0.6f, 0.25f},
            {"minecraft:ocelot", 0.85f, 0.85f},
            {"minecraft:panda", 0.85f, 0.95f},
            {"minecraft:parrot", 0.85f, 0.85f},
            {"minecraft:pig", 0.85f, 0.85f},
            {"minecraft:polar_bear", 0.85f, 0.85f},
            // its constructor leaves the head unset (sub_14616A9B0)
            {"minecraft:pufferfish", 0.0f, 0.0f},
            {"minecraft:rabbit", 0.85f, 0.85f},
            {"minecraft:sheep", 0.85f, 0.85f},
            {"minecraft:sniffer", 0.85f, 0.85f},
            {"minecraft:strider", 0.85f, 0.85f},
            {"minecraft:turtle", 0.85f, 0.85f},
            {"minecraft:wolf", 0.85f, 0.85f},
    };

    // player position sits 1.62001 above the feet, lowered when sneaking (0.35), lying (0.4) or asleep (0.2);
    // the head is 0.1 below it (sub_142CED370, sub_1402B5670)
    float playerEyeHeight(const ServerPlayer &player) {
        const ActorFlags &flags = player.getFlags();
        if (flags.get(ActorFlag::Swimming) || flags.get(ActorFlag::Gliding) || flags.get(ActorFlag::Crawling))
            return 0.4f;
        if (flags.get(ActorFlag::Sneaking))
            return 1.62001f - 0.35f;
        if (flags.get(ActorFlag::Sleeping))
            return 0.2f;
        return 1.62001f;
    }

    Vector3f headOf(const EntityTarget &target) {
        const Vector3f &position = target.mActor->getPosition();
        if (target.mPlayer != nullptr)
            return Vector3f(position.x, position.y + playerEyeHeight(*target.mPlayer) - 0.1f, position.z);
        // only mobs get head offsets; other actors keep theirs at the feet
        if (dynamic_cast<const MobActor *>(target.mActor) == nullptr)
            return position;

        // the mob default: straight up, 0.85 of the box height (sub_142C1F350)
        float forward = 0.0f, up = 0.85f;
        const std::string &identifier = target.mRef->mTypeId;
        for (const HeadOffset &offset: HEAD_OFFSETS) {
            if (identifier == offset.mIdentifier) {
                forward = offset.mForward;
                up = offset.mUp;
                break;
            }
        }
        const ActorSize size = sizeOf(target);
        // turned by the head yaw: sub_1437DB6A0 mode 3
        const float yaw = target.mActor->getRotation().z * 0.017453292f;
        const float ahead = forward * size.mWidth;
        return Vector3f(position.x - ahead * std::sin(yaw), position.y + up * size.mHeight,
                        position.z + ahead * std::cos(yaw));
    }

    float wrapDegrees(float angle) {
        float wrapped = std::fmod(angle + 180.0f, 360.0f);
        if (wrapped < 0.0f)
            wrapped += 360.0f;
        return wrapped - 180.0f;
    }

    // both angles wrapped to [-180, 180), never clamped (sub_14C351740)
    void applyRotation(const EntityTarget &target, float pitch, float yaw) {
        target.mActor->setRotation(Vector3f(wrapDegrees(pitch), wrapDegrees(yaw), 0.0f));
        if (target.mPlayer != nullptr)
            target.mPlayer->teleport(target.mApi->host(), target.mPlayer->getPosition());
        else if (target.mServerActor != nullptr)
            target.mApi->host().broadcastActorMove(*target.mServerActor);
    }

    void rotationTowards(const Vector3f &from, const Vector3f &to, float &pitch, float &yaw) {
        const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
        const float horizontal = std::sqrt(dx * dx + dz * dz);
        yaw = std::atan2(-dx, dz) * 57.29578f;
        pitch = -std::atan2(dy, horizontal) * 57.29578f;
    }

    JSValue entityLookAt(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        // from the state position: the eyes of a player, the feet of anything else; a zero direction gives 0
        // (sub_14C359D90)
        Vector3f from = target.mActor->getPosition();
        if (target.mPlayer != nullptr)
            from.y += playerEyeHeight(*target.mPlayer);
        const float dx = (float) x - from.x, dy = (float) y - from.y, dz = (float) z - from.z;
        const float epsilon = 0.00000011920929f;
        const float yaw = std::fabs(dx) > epsilon || std::fabs(dz) > epsilon ? std::atan2(dx, dz) * -57.295776f : 0.0f;
        const float pitch = dx * dx + dy * dy + dz * dz > epsilon
                                    ? std::atan2(dy, std::sqrt(dz * dz + dx * dx)) * -57.295776f
                                    : 0.0f;
        applyRotation(target, pitch, yaw);
        return JS_UNDEFINED;
    }

    Vector3f viewDirectionOf(const Actor &actor) {
        const Vector3f &rotation = actor.getRotation();
        const float pitch = rotation.x * 0.017453292f;
        const float yaw = rotation.y * 0.017453292f;
        return Vector3f(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    }

    bool readFlag(JSContext *ctx, JSValueConst options, const char *key) {
        if (!JS_IsObject(options))
            return false;
        JSValue value = JS_GetPropertyStr(ctx, options, key);
        const bool flag = JS_ToBool(ctx, value) == 1;
        JS_FreeValue(ctx, value);
        return flag;
    }

    // false when maxDistance allows no ray
    bool readRayDistance(JSContext *ctx, JSValueConst options, float &distance) {
        double requested = 0.0;
        const bool present = JS_IsObject(options) && readNumber(ctx, options, "maxDistance", requested);
        return ScriptRaycast::clampDistance(present, requested, distance);
    }

    JSValue entityGetBlockFromViewDirection(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        JSValueConst options = argc >= 1 ? argv[0] : JS_UNDEFINED;
        ScriptRaycast::BlockOptions ray;
        if (!readRayDistance(ctx, options, ray.mMaxDistance))
            return JS_UNDEFINED;
        ray.mIncludeLiquids = readFlag(ctx, options, "includeLiquidBlocks");
        ray.mIncludePassable = readFlag(ctx, options, "includePassableBlocks");
        ray.mAccepts = [ctx, options](const BlockState &state) { return blockStatePassesFilter(ctx, state, options); };

        Level &level = target.mApi->host().getLevelFor(*target.mActor);
        ScriptRaycast::BlockHit hit;
        if (!ScriptRaycast::castBlocks(level, headOf(target), viewDirectionOf(*target.mActor), ray, hit))
            return JS_UNDEFINED;

        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "block",
                          target.mApi->makeBlock(level, hit.mBlock.x, hit.mBlock.y, hit.mBlock.z));
        JS_SetPropertyStr(ctx, result, "face", JS_NewString(ctx, hit.mFace));
        JS_SetPropertyStr(ctx, result, "faceLocation",
                          vector3(ctx, hit.mFaceLocation.x, hit.mFaceLocation.y, hit.mFaceLocation.z));
        return result;
    }

    JSValue entityGetEntitiesFromViewDirection(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        JSValueConst options = argc >= 1 ? argv[0] : JS_UNDEFINED;
        JSValue array = JS_NewArray(ctx);
        float distance = 0.0f;
        if (!readRayDistance(ctx, options, distance))
            return array;

        ServerNetworkHandler &host = target.mApi->host();
        Level &level = host.getLevelFor(*target.mActor);
        const Vector3f origin = headOf(target);
        const Vector3f direction = viewDirectionOf(*target.mActor);

        // blocks stop the ray unless ignoreBlockCollision
        if (!readFlag(ctx, options, "ignoreBlockCollision")) {
            ScriptRaycast::BlockOptions ray;
            ray.mMaxDistance = distance;
            ray.mIncludeLiquids = readFlag(ctx, options, "includeLiquidBlocks");
            ray.mIncludePassable = readFlag(ctx, options, "includePassableBlocks");
            ScriptRaycast::BlockHit hit;
            if (ScriptRaycast::castBlocks(level, origin, direction, ray, hit))
                distance = std::min(distance, hit.mDistance);
        }

        std::vector<std::pair<float, JSValue>> hits;
        const auto consider = [&](Actor &candidate, JSValue value) {
            EntityTarget other;
            other.mApi = target.mApi;
            other.mRef = target.mApi->native<ScriptEntityRef>(value);
            other.mActor = &candidate;
            other.mPlayer = dynamic_cast<ServerPlayer *>(&candidate);
            other.mServerActor = dynamic_cast<ServerActor *>(&candidate);
            const ActorSize size = sizeOf(other);
            const Vector3f &position = candidate.getPosition();
            const float half = size.mWidth / 2.0f;
            const float at = ScriptRaycast::intersectBox(
                    origin, direction, Vector3f(position.x - half, position.y, position.z - half),
                    Vector3f(position.x + half, position.y + size.mHeight, position.z + half));
            if (at < 0.0f || at > distance || other.mRef == nullptr || !matchesQuery(ctx, other, options)) {
                JS_FreeValue(ctx, value);
                return;
            }
            hits.emplace_back(at, value);
        };
        for (auto &entry: host.getActors()) {
            ServerActor &actor = *entry.second;
            if (&actor != target.mActor && actor.isAlive() && actor.isIn(level))
                consider(actor, target.mApi->makeActor(actor));
        }
        for (auto &entry: host.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (&player != target.mActor && player.isSpawned() && player.isIn(level))
                consider(player, target.mApi->makePlayer(player));
        }

        std::stable_sort(hits.begin(), hits.end(),
                         [](const std::pair<float, JSValue> &left, const std::pair<float, JSValue> &right) {
                             return left.first < right.first;
                         });
        uint32_t index = 0;
        for (const auto &found: hits) {
            JSValue entry = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, entry, "distance", JS_NewFloat64(ctx, found.first));
            JS_SetPropertyStr(ctx, entry, "entity", found.second);
            JS_SetPropertyUint32(ctx, array, index++, entry);
        }
        return array;
    }

    JSValue entityGetViewDirection(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const Vector3f direction = viewDirectionOf(*entityOf(ctx, thisVal).mActor);
        return vector3(ctx, direction.x, direction.y, direction.z);
    }

    JSValue entityTriggerEvent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        MobActor *mob = dynamic_cast<MobActor *>(target.mActor);
        if (mob != nullptr)
            mob->fireEvent(target.mApi->host(), toString(ctx, argv[0]));
        return JS_UNDEFINED;
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

    void syncEntityData(const EntityTarget &target) {
        if (target.mPlayer != nullptr)
            target.mApi->host()._sendEntityData(*target.mPlayer);
        else if (target.mServerActor != nullptr)
            target.mApi->host().syncActorFlags(*target.mServerActor);
    }

    // actor flag 129 (sub_14C35A6F0, sub_14C35A770)
    JSValue entityGetNameplateDepthTested(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, entityOf(ctx, thisVal).mActor->getFlags().get(ActorFlag::NameplateDepthTested));
    }

    JSValue entitySetNameplateDepthTested(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1 || !JS_IsBool(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const bool tested = JS_ToBool(ctx, argv[0]) == 1;
        if (target.mActor->getFlags().get(ActorFlag::NameplateDepthTested) == tested)
            return JS_UNDEFINED;

        target.mActor->getFlags().set(ActorFlag::NameplateDepthTested, tested);
        syncEntityData(target);
        return JS_UNDEFINED;
    }

    // float data 140 (sub_14C356570, sub_14C356640)
    JSValue entityGetNameplateRenderDistance(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewFloat64(ctx, entityOf(ctx, thisVal).mActor->getNameplateRenderDistance());
    }

    JSValue entitySetNameplateRenderDistance(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        double distance = 0.0;
        if (argc < 1 || !JS_IsNumber(argv[0]) || JS_ToFloat64(ctx, &distance, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (target.mActor->getNameplateRenderDistance() == (float) distance)
            return JS_UNDEFINED;

        target.mActor->setNameplateRenderDistance((float) distance);
        syncEntityData(target);
        return JS_UNDEFINED;
    }

    void removeActorLater(ServerNetworkHandler &host, int64_t uniqueId) {
        host.postToMainThread([&host, uniqueId] { host.removeActor(uniqueId); });
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

    // a solid block inside the collision box at the destination
    // 2 for an unloaded chunk, 1 for a solid block, 0 when clear, like sub_14BE1B5A0
    int destinationBlocked(Level &level, const Vector3f &position, const ActorSize &size) {
        const float half = size.mWidth / 2.0f;
        int result = 0;
        for (int32_t x = (int32_t) std::floor(position.x - half); x <= (int32_t) std::floor(position.x + half); ++x) {
            for (int32_t y = (int32_t) std::floor(position.y); y <= (int32_t) std::floor(position.y + size.mHeight);
                 ++y) {
                for (int32_t z = (int32_t) std::floor(position.z - half); z <= (int32_t) std::floor(position.z + half);
                     ++z) {
                    const BlockState *state = level.peekBlockPtr(x, y, z);
                    if (state == nullptr)
                        return 2;
                    const BlockData *data = BlockDataTable::find(state->mName.c_str());
                    if (data != nullptr && data->mSolid)
                        result = 1;
                }
            }
        }
        return result;
    }

    // false when checkForBlocks found the destination blocked; throws on a malformed call
    JSValue teleportEntity(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, bool trying) {
        const EntityTarget target = entityOf(ctx, thisVal);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z) || !std::isfinite(x) || !std::isfinite(y)
            || !std::isfinite(z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        const Vector3f position((float) x, (float) y, (float) z);
        Level *destination = &target.mApi->host().getLevelFor(*target.mActor);
        Vector3f rotation = target.mActor->getRotation();
        bool rotate = false, keepVelocity = false, checkForBlocks = false;
        JSValueConst options = argc >= 2 ? argv[1] : JS_UNDEFINED;
        if (JS_IsObject(options)) {
            JSValue dimension = JS_GetPropertyStr(ctx, options, "dimension");
            if (target.mApi->native<ScriptDimensionRef>(dimension) != nullptr)
                destination = &target.mApi->levelOf(dimension);
            JS_FreeValue(ctx, dimension);

            JSValue rotationValue = JS_GetPropertyStr(ctx, options, "rotation");
            double pitch = 0.0, yaw = 0.0;
            if (JS_IsObject(rotationValue) && readNumber(ctx, rotationValue, "x", pitch)
                && readNumber(ctx, rotationValue, "y", yaw)) {
                // pitch clamped to 89 and yaw wrapped to [-180, 180) (sub_14BE16000)
                rotation = Vector3f(std::clamp((float) pitch, -89.0f, 89.0f), wrapDegrees((float) yaw), 0.0f);
                rotate = true;
            }
            JS_FreeValue(ctx, rotationValue);

            JSValue facing = JS_GetPropertyStr(ctx, options, "facingLocation");
            double fx = 0.0, fy = 0.0, fz = 0.0;
            if (JS_IsObject(facing) && readVector3(ctx, facing, fx, fy, fz)) {
                // aimed from the destination itself, not the eyes (sub_14BE16000)
                float pitch2 = 0.0f, yaw2 = 0.0f;
                rotationTowards(position, Vector3f((float) fx, (float) fy, (float) fz), pitch2, yaw2);
                rotation = Vector3f(pitch2, yaw2, 0.0f);
                rotate = true;
            }
            JS_FreeValue(ctx, facing);

            JSValue keep = JS_GetPropertyStr(ctx, options, "keepVelocity");
            keepVelocity = JS_ToBool(ctx, keep) == 1;
            JS_FreeValue(ctx, keep);
            JSValue check = JS_GetPropertyStr(ctx, options, "checkForBlocks");
            if (!JS_IsUndefined(check))
                checkForBlocks = JS_ToBool(ctx, check) == 1;
            JS_FreeValue(ctx, check);
        }

        // the game has no height check here (sub_14C35A800)
        if (keepVelocity && target.mPlayer != nullptr)
            return target.mApi->throwError("InvalidArgumentError",
                                           "keepVelocity is not supported for player teleportation.");
        // name tag then id, like sub_14C35A800
        if (!target.mActor->isIn(*destination) && target.mActor->hasPassengers()) {
            const std::string name = target.mPlayer != nullptr        ? target.mPlayer->getName()
                                     : target.mServerActor != nullptr ? target.mServerActor->getNameTag()
                                                                      : std::string();
            return target.mApi->throwError("Error", "Entity '" + name + "' (ID: " + target.mRef->mEntityId
                                                            + ") needs to dismount passengers before teleporting "
                                                              "to a different dimension!");
        }
        // only tryTeleport looks: an unloaded chunk always fails, blocks only when checkForBlocks is set
        if (trying) {
            const int blocked = destinationBlocked(*destination, position, sizeOf(target));
            if (blocked == 2 || (blocked == 1 && checkForBlocks))
                return JS_FALSE;
        }

        ServerNetworkHandler &host = target.mApi->host();
        if (rotate)
            target.mActor->setRotation(rotation);
        // the level change places the entity itself; an actor may even be unloaded by it
        if (!target.mActor->isIn(*destination)) {
            host.changeActorLevel(*target.mActor, *destination, position);
            return trying ? JS_TRUE : JS_UNDEFINED;
        }
        if (!keepVelocity)
            target.mActor->setMotion(Vector3f(0.0f, 0.0f, 0.0f));

        if (target.mPlayer != nullptr) {
            target.mPlayer->teleport(host, position);
        } else {
            target.mActor->setPosition(position);
            target.mActor->resetFallDistance();
            if (target.mServerActor != nullptr)
                host.broadcastActorMove(*target.mServerActor, true);
        }
        return trying ? JS_TRUE : JS_UNDEFINED;
    }

    JSValue entityTeleport(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return teleportEntity(ctx, thisVal, argc, argv, false);
    }

    JSValue entityTryTeleport(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return teleportEntity(ctx, thisVal, argc, argv, true);
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
        double pitch = 0.0, yaw = 0.0;
        if (argc < 1 || !JS_IsObject(argv[0]) || !readNumber(ctx, argv[0], "x", pitch)
            || !readNumber(ctx, argv[0], "y", yaw))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        applyRotation(target, (float) pitch, (float) yaw);
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

    std::string snakeCase(const std::string &camel) {
        std::string snake;
        for (const char character: camel) {
            if (character >= 'A' && character <= 'Z') {
                snake += '_';
                snake += (char) (character - 'A' + 'a');
            } else {
                snake += character;
            }
        }
        return snake;
    }

    // EntityApplyDamageOptions or EntityApplyDamageByProjectileOptions as the game's damage source
    bool readDamageSource(JSContext *ctx, ScriptApi &api, Actor &victim, JSValueConst options,
                          ActorDamageSource &source) {
        source = ActorDamageSource::environment("death.attack.generic", victim.getName());
        if (!JS_IsObject(options))
            return true;

        JSValue causeValue = JS_GetPropertyStr(ctx, options, "cause");
        JSValue entityValue = JS_GetPropertyStr(ctx, options, "damagingEntity");
        JSValue projectileValue = JS_GetPropertyStr(ctx, options, "damagingProjectile");
        Actor *damaging = api.resolveEntity(entityValue);
        Actor *projectile = api.resolveEntity(projectileValue);
        const bool hasCause = JS_IsString(causeValue);
        const std::string cause = hasCause ? snakeCase(toString(ctx, causeValue)) : std::string();
        const bool badEntity = !JS_IsUndefined(entityValue) && damaging == nullptr;
        const bool badProjectile = !JS_IsUndefined(projectileValue) && projectile == nullptr;
        JS_FreeValue(ctx, causeValue);
        JS_FreeValue(ctx, entityValue);
        JS_FreeValue(ctx, projectileValue);
        if (badEntity || badProjectile)
            return false;

        if (projectile != nullptr) {
            source.mDeathMessageKey = "death.attack.arrow";
            source.mProjectile = true;
            source.mDamager = projectile;
            if (damaging == nullptr) {
                ServerActor *shot = dynamic_cast<ServerActor *>(projectile);
                if (shot != nullptr && shot->hasOwnerPlayer())
                    damaging = api.resolvePlayerByHandle(shot->getOwnerPlayerHandle());
                else if (shot != nullptr && shot->getOwnerUniqueId() >= 0)
                    damaging = api.host().getActor(shot->getOwnerUniqueId());
            }
        } else if (hasCause) {
            const char *key = DamageCause::findDeathMessageKey(cause);
            if (key != nullptr)
                source.mDeathMessageKey = key;
            source.mCause = cause;
        }

        if (damaging != nullptr) {
            source.mAttacker = damaging;
            source.mDeathMessageParameters.push_back(damaging->getName());
            source.fromOrigin(damaging->getPosition());
            if (!hasCause && projectile == nullptr)
                source.mDeathMessageKey = damaging->isPlayer() ? "death.attack.player" : "death.attack.mob";
        }
        return true;
    }

    JSValue entityApplyDamage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        double amount = 0.0;
        if (argc < 1 || JS_ToFloat64(ctx, &amount, argv[0]) != 0 || !std::isfinite(amount))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        ActorDamageSource source;
        if (!readDamageSource(ctx, *target.mApi, *target.mActor, argc >= 2 ? argv[1] : JS_UNDEFINED, source))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        if (target.mPlayer != nullptr)
            return JS_NewBool(ctx,
                              target.mApi->host().hurt(*target.mPlayer, (float) amount, source) == DamageResult::Dealt);
        if (target.mServerActor != nullptr)
            return JS_NewBool(ctx, target.mServerActor->hurt(target.mApi->host(), (float) amount, source));
        return JS_FALSE;
    }

    JSValue entityKill(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (target.mPlayer != nullptr) {
            if (target.mPlayer->isDead())
                return JS_FALSE;
            target.mApi->host().killPlayer(*target.mPlayer, "death.attack.generic", {target.mPlayer->getName()});
            return JS_TRUE;
        }
        if (target.mServerActor != nullptr) {
            if (!target.mServerActor->isAlive())
                return JS_FALSE;
            target.mServerActor->kill(target.mApi->host(), nullptr, 0);
            return JS_TRUE;
        }
        ItemActor *item = dynamic_cast<ItemActor *>(target.mActor);
        if (item != nullptr)
            item->setRemoved(true);
        return JS_NewBool(ctx, item != nullptr);
    }

    bool readEffectType(JSContext *ctx, JSValueConst value, MobEffectId &id) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (const ScriptNamedRef *type = api->native<ScriptNamedRef>(value))
            return parseMobEffect(type->mName, id);
        return parseMobEffect(toString(ctx, value), id);
    }

    JSValue entityAddEffect(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 2)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        MobEffectId id;
        if (!readEffectType(ctx, argv[0], id))
            return target.mApi->throwError("InvalidArgumentError", "Invalid effect type");

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

        MobEffectInstance instance;
        instance.mId = id;
        instance.mDuration = duration;
        instance.mAmplifier = amplifier;
        instance.mParticles = showParticles;
        if (!target.mActor->addEffect(instance))
            return JS_UNDEFINED;

        // players send their effect changes themselves
        if (target.mServerActor != nullptr) {
            const MobEffectInstance *applied = target.mActor->getEffect(id);
            if (applied != nullptr)
                target.mApi->host().applyActorEffect(*target.mServerActor, (int32_t) id, applied->mAmplifier,
                                                     applied->mDuration, applied->mParticles);
        }
        return target.mApi->makeEffect(*target.mActor, id);
    }

    JSValue entityGetEffect(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        MobEffectId id;
        if (argc < 1 || !readEffectType(ctx, argv[0], id))
            return target.mApi->throwError("InvalidArgumentError", "Invalid effect type");
        if (target.mActor->getEffect(id) == nullptr)
            return JS_UNDEFINED;
        return target.mApi->makeEffect(*target.mActor, id);
    }

    JSValue entityGetEffects(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        std::vector<int32_t> ids;
        for (const auto &entry: target.mActor->getEffects().getAll())
            ids.push_back(entry.first);
        std::sort(ids.begin(), ids.end());

        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const int32_t id: ids)
            JS_SetPropertyUint32(ctx, array, index++, target.mApi->makeEffect(*target.mActor, (MobEffectId) id));
        return array;
    }

    JSValue entityRemoveEffect(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        MobEffectId id;
        if (argc < 1 || !readEffectType(ctx, argv[0], id))
            return target.mApi->throwError("InvalidArgumentError", "Invalid effect type");

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
        const EntityTarget target = entityOf(ctx, thisVal);
        if (!target.mActor->isOnFire())
            return JS_FALSE;
        target.mActor->extinguish();
        if (target.mPlayer != nullptr)
            target.mApi->host()._sendEntityData(*target.mPlayer);
        else if (target.mServerActor != nullptr)
            target.mApi->host().syncActorFlags(*target.mServerActor);
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
        return target.mApi->runCommand(target.mApi->host().getLevelFor(*target.mActor), target.mActor, line);
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

        return propertyValue(ctx, *target.mServerActor, *descriptor);
    }

    JSValue propertyValue(JSContext *ctx, ServerActor &actor, const ActorPropertyDescription &descriptor) {
        const std::string &name = descriptor.mName;
        if (descriptor.mType == ActorPropertyDescription::Type::Float)
            return JS_NewFloat64(ctx, actor.getFloatProperty(name, descriptor.mDefaultFloat));
        if (descriptor.mType == ActorPropertyDescription::Type::Bool)
            return JS_NewBool(ctx, actor.getIntProperty(name, descriptor.mDefaultInt) != 0);
        if (descriptor.mType == ActorPropertyDescription::Type::Enum) {
            const int32_t index = actor.getIntProperty(name, descriptor.mDefaultInt);
            if (index >= 0 && index < (int32_t) descriptor.mEnumValues.size())
                return string(ctx, descriptor.mEnumValues[index]);
            return JS_UNDEFINED;
        }
        return JS_NewInt32(ctx, actor.getIntProperty(name, descriptor.mDefaultInt));
    }

    JSValue entityResetProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1 || target.mServerActor == nullptr)
            return JS_UNDEFINED;

        const ActorPropertyDescription *descriptor = target.mServerActor->findPropertyDescription(
                toString(ctx, argv[0]));
        if (descriptor == nullptr)
            return JS_UNDEFINED;

        ServerActor &actor = *target.mServerActor;
        if (descriptor->mType == ActorPropertyDescription::Type::Float)
            actor.setFloatProperty(descriptor->mName, descriptor->mDefaultFloat);
        else if (descriptor->mType == ActorPropertyDescription::Type::Bool)
            actor.setIntProperty(descriptor->mName, descriptor->mDefaultBool ? 1 : 0);
        else
            actor.setIntProperty(descriptor->mName, descriptor->mDefaultInt);
        target.mApi->host().syncActorProperties(actor);
        return propertyValue(ctx, actor, *descriptor);
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


    JSValue entityGetHeadLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const Vector3f head = headOf(entityOf(ctx, thisVal));
        return vector3(ctx, head.x, head.y, head.z);
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

    JSValue entitySetDynamicProperties(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::unordered_map<std::string, DynamicPropertyValue> *store = dynamicPropertiesOf(entityOf(ctx, thisVal));
        if (argc < 1 || !JS_IsObject(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (store != nullptr && !dynamicPropertySetAll(ctx, *store, argv[0]))
            return JS_EXCEPTION;
        return JS_UNDEFINED;
    }

    JSValue entityGetDynamicPropertyTotalByteCount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::unordered_map<std::string, DynamicPropertyValue> *store = dynamicPropertiesOf(entityOf(ctx, thisVal));
        return JS_NewInt64(ctx, store == nullptr ? 0 : dynamicPropertyByteCount(*store));
    }

    JSValue entityClearDynamicProperties(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::unordered_map<std::string, DynamicPropertyValue> *store = dynamicPropertiesOf(entityOf(ctx, thisVal));
        if (store != nullptr)
            store->clear();
        return JS_UNDEFINED;
    }

    // inventories and equipment are read from the game's containers, every other component from the definition
    JSValue makeEntityComponent(const EntityTarget &target, const std::string &requested) {
        const std::string id = requested.find(':') == std::string::npos ? "minecraft:" + requested : requested;
        JSValue container = target.mApi->makeContainerComponent(*target.mActor, *target.mRef, id);
        if (!JS_IsUndefined(container))
            return container;
        return target.mApi->makeEntityComponent(*target.mActor, *target.mRef, id);
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
        std::vector<std::string> ids = ScriptApi::containerComponentIds();
        for (const std::string &id: target.mApi->entityComponentIds(*target.mActor))
            ids.push_back(id);

        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &id: ids) {
            JSValue component = makeEntityComponent(target, id);
            if (!JS_IsUndefined(component))
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

    // the identity component's playfab id, the "pfid" of the spawn log (sub_14D5F0110, sub_141EC1280)
    JSValue playerGetPersistentId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, playerOf(ctx, thisVal)->getPlayFabId());
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

    JSValue playerGetGameMode(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewString(ctx, gameModeName(entityOf(ctx, thisVal)));
    }

    JSValue playerSetGameMode(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 1 || JS_IsUndefined(argv[0]))
            return JS_UNDEFINED;

        const int32_t mode = target.mApi->parseGameMode(toString(ctx, argv[0]));
        if (mode < 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        target.mApi->host().setPlayerGameMode(*target.mPlayer, mode);
        return JS_UNDEFINED;
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

    JSValue playerSetSelectedSlot(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = playerOf(ctx, thisVal);
        int32_t slot = 0;
        if (argc < 1 || JS_ToInt32(ctx, &slot, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (slot < 0 || slot >= PlayerInventory::HOTBAR_SIZE)
            return ScriptApi::fromContext(ctx)->throwError("InvalidArgumentError", "Invalid hotbar slot");
        player->getInventory().setSelectedSlot(slot);
        player->getInventoryManager().syncSelectedHotbarSlot();
        return JS_UNDEFINED;
    }

    enum class PlayerState { Flying, Gliding, Jumping, Emoting };

    JSValue playerGetState(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ServerPlayer *player = playerOf(ctx, thisVal);
        switch ((PlayerState) magic) {
            case PlayerState::Flying:
                return JS_NewBool(ctx, player->isFlying());
            case PlayerState::Gliding:
                return JS_NewBool(ctx, player->getFlags().get(ActorFlag::Gliding));
            case PlayerState::Jumping:
                return JS_NewBool(ctx, player->getInputState().mJumping);
            case PlayerState::Emoting:
                return JS_NewBool(ctx, player->getFlags().get(ActorFlag::Emoting));
        }
        return JS_FALSE;
    }

    enum class PlayerExperience { Total, EarnedAtLevel, NeededForNext };

    JSValue playerGetExperience(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const ExperienceManager &experience = playerOf(ctx, thisVal)->getExperience();
        switch ((PlayerExperience) magic) {
            case PlayerExperience::Total:
                return JS_NewInt32(ctx, experience.getCurrentTotalXp());
            case PlayerExperience::EarnedAtLevel:
                return JS_NewInt32(ctx, experience.getRemainderXp());
            case PlayerExperience::NeededForNext:
                return JS_NewInt32(ctx, ExperienceManager::getXpToCompleteLevel(experience.getXpLevel()));
        }
        return JS_NewInt32(ctx, 0);
    }

    // operators are GameDirectors and Operator, everyone else Any and Member
    JSValue playerGetCommandPermissionLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, playerOf(ctx, thisVal)->isOp() ? 1 : 0);
    }

    JSValue playerGetPlayerPermissionLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, playerOf(ctx, thisVal)->isOp() ? 2 : 1);
    }

    JSValue playerGetSpawnPoint(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        ServerPlayer &player = *target.mPlayer;
        if (!player.hasSpawnPoint())
            return JS_UNDEFINED;

        const Vector3i &point = player.getSpawnStandPosition();
        World *world = target.mApi->host().getWorlds().find(player.getSpawnWorld());
        World &owner = world == nullptr ? target.mApi->host().getWorldFor(player) : *world;
        JSValue location = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, location, "dimension",
                          target.mApi->makeDimension(owner.getLevel(player.getSpawnDimension())));
        JS_SetPropertyStr(ctx, location, "x", JS_NewInt32(ctx, point.x));
        JS_SetPropertyStr(ctx, location, "y", JS_NewInt32(ctx, point.y));
        JS_SetPropertyStr(ctx, location, "z", JS_NewInt32(ctx, point.z));
        return location;
    }

    JSValue playerSetSpawnPoint(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        ServerPlayer &player = *target.mPlayer;
        if (argc < 1 || JS_IsUndefined(argv[0])) {
            player.clearSpawnPoint();
            return JS_UNDEFINED;
        }

        double x = 0.0, y = 0.0, z = 0.0;
        JSValue dimension = JS_IsObject(argv[0]) ? JS_GetPropertyStr(ctx, argv[0], "dimension") : JS_UNDEFINED;
        const bool valid = readVector3(ctx, argv[0], x, y, z) && target.mApi->native<ScriptDimensionRef>(dimension);
        Level &level = valid ? target.mApi->levelOf(dimension) : target.mApi->host().getLevelFor(player);
        JS_FreeValue(ctx, dimension);
        if (!valid)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (y < level.getMinY() || y > level.getMaxY())
            return target.mApi->throwError("LocationOutOfWorldBoundariesError", "");

        const Vector3i point((int32_t) std::floor(x), (int32_t) std::floor(y), (int32_t) std::floor(z));
        player.setSpawnPosition(point, level.getDimensionType(), target.mApi->host().getWorldOf(level).getName());
        return JS_UNDEFINED;
    }

    JSValue playerGetInputInfo(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        ScriptInputInfoRef *info = new ScriptInputInfoRef();
        info->mHandle = (uint32_t) target.mRef->mId;
        return target.mApi->makeInstance("InputInfo", info);
    }

    ServerPlayer *inputOwner(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptInputInfoRef *info = api->native<ScriptInputInfoRef>(thisVal);
        ServerPlayer *player = info == nullptr ? nullptr : api->resolvePlayerByHandle(info->mHandle);
        if (player == nullptr)
            api->throwError("InvalidEntityError", "The player has left.");
        return player;
    }

    const char *const INPUT_MODES[] = {nullptr, "KeyboardAndMouse", "Touch", "Gamepad", "MotionController"};

    JSValue inputGetLastMode(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = inputOwner(ctx, thisVal);
        if (player == nullptr)
            return JS_EXCEPTION;
        const int32_t mode = player->getInputState().mInputMode;
        return JS_NewString(ctx, mode >= 1 && mode <= 4 ? INPUT_MODES[mode] : "KeyboardAndMouse");
    }

    JSValue inputGetButtonState(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = inputOwner(ctx, thisVal);
        if (player == nullptr)
            return JS_EXCEPTION;
        const std::string button = argc >= 1 ? toString(ctx, argv[0]) : std::string();
        if (button != "Jump" && button != "Sneak")
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const ServerPlayer::InputState &state = player->getInputState();
        const bool pressed = button == "Jump" ? state.mJumpHeld : state.mSneakHeld;
        return JS_NewString(ctx, pressed ? "Pressed" : "Released");
    }

    JSValue inputGetMovementVector(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = inputOwner(ctx, thisVal);
        if (player == nullptr)
            return JS_EXCEPTION;
        JSValue vector = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, vector, "x", JS_NewFloat64(ctx, player->getInputState().mMoveX));
        JS_SetPropertyStr(ctx, vector, "y", JS_NewFloat64(ctx, player->getInputState().mMoveY));
        return vector;
    }

    JSValue effectOf(JSContext *ctx, JSValueConst thisVal, const MobEffectInstance **out) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptEffectRef *ref = api->native<ScriptEffectRef>(thisVal);
        Actor *actor = ref == nullptr ? nullptr : api->resolveEntity(ref->mEntity);
        *out = actor == nullptr ? nullptr : actor->getEffect((MobEffectId) ref->mEffectId);
        if (*out == nullptr)
            return api->throwError("InvalidEntityError", "The effect is no longer active.");
        return JS_UNDEFINED;
    }

    enum class EffectField { TypeId, Amplifier, Duration, DisplayName };

    // sub_1435541B0: the description id each effect registers with
    const char *effectDescriptionId(const std::string &name) {
        static const std::unordered_map<std::string, const char *> ids = {
                {"speed", "potion.moveSpeed"},
                {"slowness", "potion.moveSlowdown"},
                {"haste", "potion.digSpeed"},
                {"mining_fatigue", "potion.digSlowDown"},
                {"strength", "potion.damageBoost"},
                {"instant_health", "potion.heal"},
                {"instant_damage", "potion.harm"},
                {"jump_boost", "potion.jump"},
                {"nausea", "potion.confusion"},
                {"regeneration", "potion.regeneration"},
                {"resistance", "potion.resistance"},
                {"fire_resistance", "potion.fireResistance"},
                {"water_breathing", "potion.waterBreathing"},
                {"invisibility", "potion.invisibility"},
                {"blindness", "potion.blindness"},
                {"night_vision", "potion.nightVision"},
                {"hunger", "potion.hunger"},
                {"weakness", "potion.weakness"},
                {"poison", "potion.poison"},
                {"wither", "potion.wither"},
                {"health_boost", "potion.healthBoost"},
                {"absorption", "potion.absorption"},
                {"saturation", "potion.saturation"},
                {"levitation", "potion.levitation"},
                {"fatal_poison", "potion.poison"},
                {"conduit_power", "potion.conduitPower"},
                {"slow_falling", "potion.slowFalling"},
                {"bad_omen", "effect.badOmen"},
                {"village_hero", "effect.villageHero"},
                {"darkness", "effect.darkness"},
                {"trial_omen", "effect.trial_omen"},
                {"wind_charged", "effect.wind_charged"},
                {"weaving", "effect.weaving"},
                {"oozing", "effect.oozing"},
                {"infested", "effect.infested"},
                {"raid_omen", "effect.raid_omen"},
                {"breath_of_the_nautilus", "effect.customNautilus"},
                {"freezing", "effect.freezing"},
        };
        const auto found = ids.find(name);
        return found == ids.end() ? "" : found->second;
    }

    // sub_14355CDA0: the translated name, then the translated potency for amplifiers 1 to 5
    std::string effectDisplayName(const MobEffectInstance &effect) {
        const VanillaTexts &texts = VanillaTexts::getInstance();
        std::string name = texts.translate(effectDescriptionId(ScriptApi::effectTypeId(effect.mId)));
        if (effect.mAmplifier >= 1 && effect.mAmplifier <= 5)
            name += " " + texts.translate("potion.potency." + std::to_string(effect.mAmplifier));
        return name;
    }

    JSValue effectGetField(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const MobEffectInstance *effect = nullptr;
        JSValue error = effectOf(ctx, thisVal, &effect);
        if (effect == nullptr)
            return error;
        switch ((EffectField) magic) {
            case EffectField::TypeId:
                return string(ctx, ScriptApi::effectTypeId(effect->mId));
            case EffectField::Amplifier:
                return JS_NewInt32(ctx, effect->mAmplifier);
            case EffectField::Duration:
                return JS_NewInt32(ctx, effect->mInfinite ? -1 : effect->mDuration);
            case EffectField::DisplayName:
                return string(ctx, effectDisplayName(*effect));
        }
        return JS_UNDEFINED;
    }

    JSValue effectGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptEffectRef *ref = api->native<ScriptEffectRef>(thisVal);
        Actor *actor = ref == nullptr ? nullptr : api->resolveEntity(ref->mEntity);
        return JS_NewBool(ctx, actor != nullptr && actor->getEffect((MobEffectId) ref->mEffectId) != nullptr);
    }

    JSValue namedGetName(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, ScriptApi::fromContext(ctx)->native<ScriptNamedRef>(thisVal)->mName);
    }

    JSValue entityTypeGetLocalizationKey(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, localizationKeyOf(ScriptApi::fromContext(ctx)->native<ScriptNamedRef>(thisVal)->mName));
    }

    JSValue makeNamed(ScriptApi *api, const char *className, const std::string &name) {
        ScriptNamedRef *ref = new ScriptNamedRef();
        ref->mName = name;
        return api->makeInstance(className, ref);
    }

    std::vector<MobEffectId> knownEffects() {
        std::vector<MobEffectId> ids;
        for (int32_t raw = 1; raw <= MAX_MOB_EFFECT_ID; ++raw) {
            if (isKnownMobEffect(raw))
                ids.push_back((MobEffectId) raw);
        }
        return ids;
    }

    JSValue effectTypesGet(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        MobEffectId id;
        if (argc < 1 || !parseMobEffect(toString(ctx, argv[0]), id))
            return JS_UNDEFINED;
        return makeNamed(ScriptApi::fromContext(ctx), "EffectType", ScriptApi::effectTypeId(id));
    }

    JSValue effectTypesGetAll(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const MobEffectId id: knownEffects())
            JS_SetPropertyUint32(ctx, array, index++,
                                 makeNamed(ScriptApi::fromContext(ctx), "EffectType", ScriptApi::effectTypeId(id)));
        return array;
    }

    std::vector<std::string> entityTypeIds() {
        std::vector<std::string> ids;
        const char *const *vanilla = VanillaActorTable::getIdentifiers();
        for (size_t i = 0; i < VanillaActorTable::getCount(); ++i)
            ids.emplace_back(vanilla[i]);
        for (const CustomActorDefinition &definition: CustomContentRegistry::getInstance().getActors()) {
            if (std::find(ids.begin(), ids.end(), definition.mIdentifier) == ids.end())
                ids.push_back(definition.mIdentifier);
        }
        return ids;
    }

    JSValue entityTypesGet(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;
        std::string id = toString(ctx, argv[0]);
        if (id.find(':') == std::string::npos)
            id = "minecraft:" + id;
        const std::vector<std::string> ids = entityTypeIds();
        if (std::find(ids.begin(), ids.end(), id) == ids.end())
            return JS_UNDEFINED;
        return makeNamed(ScriptApi::fromContext(ctx), "EntityType", id);
    }

    JSValue entityTypesGetAll(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &id: entityTypeIds())
            JS_SetPropertyUint32(ctx, array, index++, makeNamed(ScriptApi::fromContext(ctx), "EntityType", id));
        return array;
    }

    JSValue playerSetCommandPermissionLevel(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        int32_t level = 0;
        if (argc < 1 || JS_ToInt32(ctx, &level, argv[0]) != 0 || level < 0 || level > 4)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        // one operator rank here: GameDirectors and above are operators
        target.mApi->host().setPlayerOp(*target.mPlayer, level >= 1);
        return JS_UNDEFINED;
    }

    enum class ChatField { NamePrefix, NameSuffix, MessagePrefix };

    std::string &chatField(ServerPlayer &player, int magic) {
        ServerPlayer::ChatStyle &style = player.getChatStyle();
        if ((ChatField) magic == ChatField::NamePrefix)
            return style.mNamePrefix;
        if ((ChatField) magic == ChatField::NameSuffix)
            return style.mNameSuffix;
        return style.mMessagePrefix;
    }

    JSValue playerGetChat(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const std::string &value = chatField(*playerOf(ctx, thisVal), magic);
        return value.empty() ? JS_UNDEFINED : string(ctx, value);
    }

    JSValue playerSetChat(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        chatField(*playerOf(ctx, thisVal), magic) = argc < 1 || JS_IsUndefined(argv[0]) ? std::string()
                                                                                        : toString(ctx, argv[0]);
        return JS_UNDEFINED;
    }

    JSValue playerGetChatDisplayName(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = playerOf(ctx, thisVal);
        const ServerPlayer::ChatStyle &style = player->getChatStyle();
        return string(ctx, style.mNamePrefix + player->getName() + style.mNameSuffix);
    }

    JSValue playerStopSound(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int all) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (all == 0 && argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        StopSoundPacket packet;
        packet.mSoundName = all != 0 ? std::string() : toString(ctx, argv[0]);
        packet.mStoppingAllSound = all != 0;
        packet.mStopMusicLegacy = false;
        target.mApi->host().sendPacketTo(target.mPlayer->getNetworkIdentifier(), packet);
        return JS_UNDEFINED;
    }

    // sub_1443AA2B0: the id and value go out as a ScriptMessagePacket
    JSValue playerPostClientMessage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        if (argc < 2)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ScriptMessagePacket packet;
        packet.mChannel = toString(ctx, argv[0]);
        packet.mMessage = toString(ctx, argv[1]);
        target.mApi->host().sendPacketTo(target.mPlayer->getNetworkIdentifier(), packet);
        return JS_UNDEFINED;
    }

    JSValue playerGetPing(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EntityTarget target = entityOf(ctx, thisVal);
        const int ping = target.mApi->host().getNetworkHandler().getAveragePing(target.mPlayer->getNetworkIdentifier());
        if (ping < 0)
            return target.mApi->throwError("EngineError", "The player's ping is not known yet.");
        return JS_NewInt32(ctx, ping);
    }

    JSValue playerEatItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const EntityTarget target = entityOf(ctx, thisVal);
        const ScriptItemRef *item = argc >= 1 ? target.mApi->native<ScriptItemRef>(argv[0]) : nullptr;
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ItemStack stack;
        if (!target.mApi->toItemStack(*item->mItem, stack) || !target.mApi->host().applyFood(*target.mPlayer, stack))
            return target.mApi->throwError("Error", "Item is not a food item");
        target.mApi->host().syncPlayerAttributes(*target.mPlayer);
        return JS_UNDEFINED;
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

    void sendSubtitle(JSContext *ctx, ServerPlayer &player, JSValueConst subtitle) {
        bool isJson = false;
        const std::string text = messageToRawText(ctx, subtitle, isJson);
        player.sendSubtitle(text, isJson);
    }

    // ScriptScreenDisplay::setTitle (sub_14B9171E0): the times first, 10/70/20 without options, then the
    // subtitle and the title; an empty title string clears it
    JSValue screenSetTitle(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player == nullptr || argc < 1)
            return JS_UNDEFINED;

        ScriptApi *api = ScriptApi::fromContext(ctx);
        const bool hasOptions = argc >= 2 && JS_IsObject(argv[1]);
        if (hasOptions) {
            int32_t durations[3] = {0, 0, 0};
            const char *const keys[3] = {"fadeInDuration", "stayDuration", "fadeOutDuration"};
            for (int i = 0; i < 3; ++i) {
                JSValue value = JS_GetPropertyStr(ctx, argv[1], keys[i]);
                const bool converted = JS_IsNumber(value) && JS_ToInt32(ctx, &durations[i], value) == 0;
                JS_FreeValue(ctx, value);
                if (!converted)
                    return JS_ThrowTypeError(ctx, "Native type conversion failed.");
                if (durations[i] < 0)
                    return api->throwError("ArgumentOutOfBoundsError", "");
            }
            player->setTitleTimes(durations[0], durations[1], durations[2]);

            JSValue subtitle = JS_GetPropertyStr(ctx, argv[1], "subtitle");
            if (!JS_IsUndefined(subtitle))
                sendSubtitle(ctx, *player, subtitle);
            JS_FreeValue(ctx, subtitle);
        } else {
            player->setTitleTimes(10, 70, 20);
        }

        bool isJson = false;
        const std::string text = messageToRawText(ctx, argv[0], isJson);
        if (!isJson && text.empty())
            player->clearTitle();
        else
            api->host().sendTitle(*player, text, isJson);
        return JS_UNDEFINED;
    }

    JSValue screenUpdateSubtitle(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player != nullptr && argc >= 1)
            sendSubtitle(ctx, *player, argv[0]);
        return JS_UNDEFINED;
    }

    bool readHudElement(JSContext *ctx, JSValueConst value, int32_t &element) {
        return JS_IsNumber(value) && JS_ToInt32(ctx, &element, value) == 0 && element >= 0
               && element < ServerPlayer::HUD_ELEMENT_COUNT;
    }

    // undefined means every element
    bool readHudElements(JSContext *ctx, JSValueConst value, bool &all, std::vector<int32_t> &elements) {
        all = JS_IsUndefined(value);
        if (all)
            return true;
        if (!JS_IsArray(value))
            return false;

        JSValue lengthValue = JS_GetPropertyStr(ctx, value, "length");
        uint32_t length = 0;
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (uint32_t index = 0; index < length; ++index) {
            JSValue item = JS_GetPropertyUint32(ctx, value, index);
            int32_t element = 0;
            const bool valid = readHudElement(ctx, item, element);
            JS_FreeValue(ctx, item);
            if (!valid)
                return false;
            elements.push_back(element);
        }
        return true;
    }

    JSValue screenSetHudVisibility(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        int32_t visibility = 0;
        bool all = true;
        std::vector<int32_t> elements;
        if (argc < 1 || !JS_IsNumber(argv[0]) || JS_ToInt32(ctx, &visibility, argv[0]) != 0 || visibility < 0
            || visibility > 1 || !readHudElements(ctx, argc >= 2 ? argv[1] : JS_UNDEFINED, all, elements))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (player != nullptr)
            player->setHudVisibility(visibility == 0, all ? nullptr : &elements);
        return JS_UNDEFINED;
    }

    // sub_141031010: everything but the listed elements is hidden; those keep their state
    JSValue screenHideAllExcept(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        bool all = true;
        std::vector<int32_t> kept;
        if (!readHudElements(ctx, argc >= 1 ? argv[0] : JS_UNDEFINED, all, kept))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (player == nullptr)
            return JS_UNDEFINED;
        if (all) {
            player->setHudVisibility(true);
            return JS_UNDEFINED;
        }

        std::vector<int32_t> hidden;
        for (int32_t element = 0; element < ServerPlayer::HUD_ELEMENT_COUNT; ++element) {
            if (std::find(kept.begin(), kept.end(), element) == kept.end())
                hidden.push_back(element);
        }
        player->setHudVisibility(true, &hidden);
        return JS_UNDEFINED;
    }

    JSValue screenResetHudElementsVisibility(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player != nullptr)
            player->setHudVisibility(false);
        return JS_UNDEFINED;
    }

    JSValue screenIsForcedHidden(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        int32_t element = 0;
        if (argc < 1 || !readHudElement(ctx, argv[0], element))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ServerPlayer *player = partOwner(ctx, thisVal);
        return JS_NewBool(ctx, player != nullptr && player->isHudElementHidden(element));
    }

    JSValue screenGetHiddenHudElements(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        JSValue result = JS_NewArray(ctx);
        uint32_t index = 0;
        for (int32_t element = 0; player != nullptr && element < ServerPlayer::HUD_ELEMENT_COUNT; ++element) {
            if (player->isHudElementHidden(element))
                JS_SetPropertyUint32(ctx, result, index++, JS_NewInt32(ctx, element));
        }
        return result;
    }

    JSValue partGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, partOwner(ctx, thisVal) != nullptr);
    }

    // TODO: camera splines and the locator bar do nothing yet
    JSValue partNoop(JSContext *, JSValueConst, int, JSValueConst *) {
        return JS_UNDEFINED;
    }

    // InputPermissionCategory values from the typings: 1 to 12, 3 unused
    bool readInputCategory(JSContext *ctx, JSValueConst value, int32_t &category) {
        return JS_IsNumber(value) && JS_ToInt32(ctx, &category, value) == 0 && category >= 1 && category <= 12
               && category != 3;
    }

    JSValue inputPermissionEnabled(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        int32_t category = 0;
        if (argc < 1 || !readInputCategory(ctx, argv[0], category))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ServerPlayer *player = partOwner(ctx, thisVal);
        return JS_NewBool(ctx, player == nullptr || !player->isInputLocked(category));
    }

    template <int32_t CATEGORY>
    JSValue inputCategoryGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        return JS_NewBool(ctx, player == nullptr || !player->isInputLocked(CATEGORY));
    }

    template <int32_t CATEGORY>
    JSValue inputCategorySet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1 || !JS_IsBool(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player != nullptr)
            player->setInputLocked(CATEGORY, !JS_ToBool(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue inputPermissionSet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        int32_t category = 0;
        if (argc < 2 || !readInputCategory(ctx, argv[0], category) || !JS_IsBool(argv[1]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player != nullptr)
            player->setInputLocked(category, !JS_ToBool(ctx, argv[1]));
        return JS_UNDEFINED;
    }

    // a property of an options bag, undefined when the bag or the property is missing
    JSValue optionOf(JSContext *ctx, JSValueConst options, const char *key) {
        return JS_IsObject(options) ? JS_GetPropertyStr(ctx, options, key) : JS_UNDEFINED;
    }

    bool readNumber(JSContext *ctx, JSValueConst value, double &out) {
        return JS_IsNumber(value) && JS_ToFloat64(ctx, &out, value) == 0;
    }

    bool readVector2(JSContext *ctx, JSValueConst value, float &x, float &y) {
        if (!JS_IsObject(value))
            return false;
        JSValue xValue = JS_GetPropertyStr(ctx, value, "x");
        JSValue yValue = JS_GetPropertyStr(ctx, value, "y");
        double readX = 0.0, readY = 0.0;
        const bool ok = readNumber(ctx, xValue, readX) && readNumber(ctx, yValue, readY);
        JS_FreeValue(ctx, xValue);
        JS_FreeValue(ctx, yValue);
        x = (float) readX;
        y = (float) readY;
        return ok;
    }

    bool readVector3f(JSContext *ctx, JSValueConst value, Vector3f &out) {
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, value, x, y, z))
            return false;
        out = Vector3f((float) x, (float) y, (float) z);
        return true;
    }

    // EasingType names the CameraEase values in the same order, InSine for EaseInSine
    bool readEase(JSContext *ctx, JSValueConst value, CameraEase &out) {
        static const char *const NAMES[] = {
                "Linear",     "Spring",     "InSine",      "OutSine",    "InOutSine",    "InQuad",     "OutQuad",
                "InOutQuad",  "InCubic",    "OutCubic",    "InOutCubic", "InQuart",      "OutQuart",   "InOutQuart",
                "InQuint",    "OutQuint",   "InOutQuint",  "InExpo",     "OutExpo",      "InOutExpo",  "InCirc",
                "OutCirc",    "InOutCirc",  "InBack",      "OutBack",    "InOutBack",    "InElastic",  "OutElastic",
                "InOutElastic", "InBounce", "OutBounce",   "InOutBounce"};
        if (!JS_IsString(value))
            return false;
        const std::string name = toString(ctx, value);
        for (size_t index = 0; index < sizeof(NAMES) / sizeof(NAMES[0]); ++index) {
            if (name == NAMES[index]) {
                out = (CameraEase) index;
                return true;
            }
        }
        return false;
    }

    // a missing time or type stays 0 and Linear: the game's options bag starts zeroed
    bool readEaseOptions(JSContext *ctx, JSValueConst options, CameraEaseData &ease) {
        JSValue easeOptions = optionOf(ctx, options, "easeOptions");
        bool ok = true;
        if (JS_IsObject(easeOptions)) {
            ease.mHasValue = true;
            JSValue time = JS_GetPropertyStr(ctx, easeOptions, "easeTime");
            JSValue type = JS_GetPropertyStr(ctx, easeOptions, "easeType");
            double seconds = 0.0;
            if (!JS_IsUndefined(time))
                ok = readNumber(ctx, time, seconds);
            ease.mTime = (float) seconds;
            if (ok && !JS_IsUndefined(type))
                ok = readEase(ctx, type, ease.mEaseType);
            JS_FreeValue(ctx, time);
            JS_FreeValue(ctx, type);
        } else if (!JS_IsUndefined(easeOptions)) {
            ok = false;
        }
        JS_FreeValue(ctx, easeOptions);
        return ok;
    }

    Actor *readEntity(JSContext *ctx, JSValueConst value) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptEntityRef *ref = api->native<ScriptEntityRef>(value);
        return ref == nullptr ? nullptr : api->resolveEntity(*ref);
    }

    void sendCamera(JSContext *ctx, ServerPlayer &player, const CameraInstructionPacket &packet) {
        ScriptApi::fromContext(ctx)->host().sendPacketTo(player.getNetworkIdentifier(), packet);
    }

    // ScriptCamera::setCamera (sub_14C871D00): the preset, then whichever options the bag holds
    JSValue cameraSetCamera(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsString(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const int preset = cameraPresetRuntimeId(toString(ctx, argv[0]));
        if (preset < 0)
            return api->throwError("Error", "Invalid camera preset.");
        if (player == nullptr)
            return JS_UNDEFINED;

        CameraInstructionPacket packet;
        packet.mHasSetInstruction = true;
        CameraSetInstruction &set = packet.mSetInstruction;
        set.mPresetRuntimeId = preset;
        const JSValueConst options = argc >= 2 ? argv[1] : JS_UNDEFINED;
        if (!readEaseOptions(ctx, options, set.mEase))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        JSValue location = optionOf(ctx, options, "location");
        JSValue facingLocation = optionOf(ctx, options, "facingLocation");
        JSValue rotation = optionOf(ctx, options, "rotation");
        JSValue facingEntity = optionOf(ctx, options, "facingEntity");
        JSValue targetEntity = optionOf(ctx, options, "targetEntity");
        JSValue targetOffset = optionOf(ctx, options, "offsetFromTargetCenter");
        JSValue entityOffset = optionOf(ctx, options, "entityOffset");
        JSValue viewOffset = optionOf(ctx, options, "viewOffset");
        const auto release = [&]() {
            for (JSValue value: {location, facingLocation, rotation, facingEntity, targetEntity, targetOffset,
                                 entityOffset, viewOffset})
                JS_FreeValue(ctx, value);
        };

        bool ok = true;
        const char *error = nullptr;
        if (!JS_IsUndefined(location))
            ok = (set.mHasPos = readVector3f(ctx, location, set.mPos));
        if (ok && !JS_IsUndefined(facingLocation))
            ok = (set.mHasFacing = readVector3f(ctx, facingLocation, set.mFacing));
        if (ok && !JS_IsUndefined(rotation)) {
            ok = (set.mHasRot = readVector2(ctx, rotation, set.mRot.x, set.mRot.y));
            if (ok && std::fabs(set.mRot.x) > 90.0f)
                error = "Pitch (x rot) is outside accepted range of [-90, 90].";
        }
        if (ok && error == nullptr && !JS_IsUndefined(facingEntity)) {
            Actor *facing = readEntity(ctx, facingEntity);
            if (facing == nullptr) {
                error = "Invalid actor selected.";
            } else {
                set.mHasFacing = true;
                set.mFacing = facing->getPosition();
            }
        }
        if (ok && error == nullptr && !JS_IsUndefined(targetEntity)) {
            Actor *target = readEntity(ctx, targetEntity);
            if (target == nullptr) {
                error = "Invalid actor selected.";
            } else {
                packet.mHasTargetInstruction = true;
                packet.mTargetInstruction.mUniqueActorId = target->getUniqueId();
                if (!JS_IsUndefined(targetOffset))
                    ok = (packet.mTargetInstruction.mHasTargetCenterOffset =
                                  readVector3f(ctx, targetOffset, packet.mTargetInstruction.mTargetCenterOffset));
            }
        }
        if (ok && error == nullptr && !JS_IsUndefined(entityOffset))
            ok = (set.mHasActorOffset = readVector3f(ctx, entityOffset, set.mActorOffset));
        if (ok && error == nullptr && !JS_IsUndefined(viewOffset))
            ok = (set.mHasViewOffset = readVector2(ctx, viewOffset, set.mViewOffset.x, set.mViewOffset.y));
        release();

        if (!ok)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (error != nullptr)
            return api->throwError("Error", error);
        sendCamera(ctx, *player, packet);
        return JS_UNDEFINED;
    }

    // sub_14C913F70: the preset becomes the player's default camera
    JSValue cameraSetDefaultCamera(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsString(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const int preset = cameraPresetRuntimeId(toString(ctx, argv[0]));
        if (preset < 0)
            return api->throwError("Error", "Invalid camera preset!");

        CameraInstructionPacket packet;
        packet.mHasSetInstruction = true;
        packet.mSetInstruction.mPresetRuntimeId = preset;
        packet.mSetInstruction.mHasDefaultPreset = true;
        packet.mSetInstruction.mDefaultPreset = true;
        if (argc >= 2 && !JS_IsUndefined(argv[1])) {
            CameraEaseData &ease = packet.mSetInstruction.mEase;
            JSValue wrapper = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, wrapper, "easeOptions", JS_DupValue(ctx, argv[1]));
            const bool ok = readEaseOptions(ctx, wrapper, ease);
            JS_FreeValue(ctx, wrapper);
            if (!ok)
                return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        }
        if (player != nullptr)
            sendCamera(ctx, *player, packet);
        return JS_UNDEFINED;
    }

    // ScriptCamera::fade (sub_14C910F70): times within [0, 10], colour channels within [0, 1]
    JSValue cameraFade(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const JSValueConst options = argc >= 1 ? argv[0] : JS_UNDEFINED;
        CameraInstructionPacket packet;
        packet.mHasFadeInstruction = true;
        CameraFadeInstruction &fade = packet.mFadeInstruction;

        JSValue time = optionOf(ctx, options, "fadeTime");
        JSValue color = optionOf(ctx, options, "fadeColor");
        bool ok = true;
        const char *error = nullptr;
        if (!JS_IsUndefined(time)) {
            double values[3] = {0.0, 0.0, 0.0};
            const char *const keys[3] = {"fadeInTime", "holdTime", "fadeOutTime"};
            for (int i = 0; ok && i < 3; ++i) {
                JSValue value = optionOf(ctx, time, keys[i]);
                ok = readNumber(ctx, value, values[i]);
                JS_FreeValue(ctx, value);
                if (ok && (values[i] < 0.0 || values[i] > 10.0))
                    error = "Fade times cannot be negative or greater than 10.";
            }
            fade.mHasTimeData = true;
            fade.mTimeData.mFadeInTime = (float) values[0];
            fade.mTimeData.mWaitTime = (float) values[1];
            fade.mTimeData.mFadeOutTime = (float) values[2];
        }
        if (ok && error == nullptr && !JS_IsUndefined(color)) {
            double values[3] = {0.0, 0.0, 0.0};
            const char *const keys[3] = {"red", "green", "blue"};
            for (int i = 0; ok && i < 3; ++i) {
                JSValue value = optionOf(ctx, color, keys[i]);
                ok = readNumber(ctx, value, values[i]);
                JS_FreeValue(ctx, value);
                if (ok && (values[i] < 0.0 || values[i] > 1.0))
                    error = "Color values cannot be less than 0 or greater than 1.";
            }
            fade.mHasColor = true;
            fade.mColor.mRed = (float) values[0];
            fade.mColor.mGreen = (float) values[1];
            fade.mColor.mBlue = (float) values[2];
        }
        JS_FreeValue(ctx, time);
        JS_FreeValue(ctx, color);

        if (!ok)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (error != nullptr)
            return api->throwError("Error", error);
        if (player != nullptr)
            sendCamera(ctx, *player, packet);
        return JS_UNDEFINED;
    }

    // ScriptCamera::setFov (sub_14C905CC0): no fov clears the custom one; a custom one lies in [30, 110]
    JSValue cameraSetFov(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const JSValueConst options = argc >= 1 ? argv[0] : JS_UNDEFINED;
        CameraInstructionPacket packet;
        packet.mHasFovInstruction = true;
        CameraFovInstruction &fov = packet.mFovInstruction;
        fov.mClear = true;

        CameraEaseData ease;
        if (!readEaseOptions(ctx, options, ease))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        fov.mEaseTime = ease.mTime;
        fov.mEaseType = ease.mEaseType;

        JSValue value = optionOf(ctx, options, "fov");
        double degrees = 0.0;
        const bool hasFov = !JS_IsUndefined(value);
        const bool ok = !hasFov || readNumber(ctx, value, degrees);
        JS_FreeValue(ctx, value);
        if (!ok)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (hasFov) {
            if (!(degrees >= 30.0 && degrees <= 110.0))
                return api->throwError("Error", "Custom FOV must be within [30.0, 110.0]");
            fov.mFov = (float) degrees;
            fov.mClear = false;
        }
        if (player != nullptr)
            sendCamera(ctx, *player, packet);
        return JS_UNDEFINED;
    }

    // ScriptCamera::attachToEntity (sub_14C919F30): without options the camera detaches
    JSValue cameraAttachToEntity(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        ScriptApi *api = ScriptApi::fromContext(ctx);
        CameraInstructionPacket packet;
        if (argc < 1 || JS_IsUndefined(argv[0])) {
            packet.mHasDetachFromActor = true;
            packet.mDetachFromActor = true;
        } else {
            JSValue entity = optionOf(ctx, argv[0], "entity");
            Actor *actor = readEntity(ctx, entity);
            JS_FreeValue(ctx, entity);
            if (actor == nullptr)
                return api->throwError("Error", "Invalid actor selected!");
            packet.mHasAttachInstruction = true;
            packet.mAttachInstruction.mUniqueActorId = actor->getUniqueId();
        }
        if (player != nullptr)
            sendCamera(ctx, *player, packet);
        return JS_UNDEFINED;
    }

    // ScriptCamera::addShake (sub_14C91C8E0): a positive duration, an intensity in (0, 4]
    JSValue cameraAddShake(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const JSValueConst options = argc >= 1 ? argv[0] : JS_UNDEFINED;
        JSValue durationValue = optionOf(ctx, options, "duration");
        JSValue intensityValue = optionOf(ctx, options, "intensity");
        JSValue typeValue = optionOf(ctx, options, "type");
        double duration = 0.0, intensity = 0.0;
        const std::string type = JS_IsString(typeValue) ? toString(ctx, typeValue) : std::string();
        const bool ok = readNumber(ctx, durationValue, duration) && readNumber(ctx, intensityValue, intensity)
                        && (type == "Positional" || type == "Rotational");
        JS_FreeValue(ctx, durationValue);
        JS_FreeValue(ctx, intensityValue);
        JS_FreeValue(ctx, typeValue);
        if (!ok)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (!(duration > 0.0))
            return api->throwError("Error", "shakeCameraOptions.duration must be > 0");
        if (!(intensity > 0.0))
            return api->throwError("Error", "shakeCameraOptions.intensity must be > 0");
        if (intensity > 4.0)
            return api->throwError("ArgumentOutOfBoundsError", "");

        CameraShakePacket packet;
        packet.mIntensity = (float) intensity;
        packet.mDuration = (float) duration;
        packet.mShakeType = type == "Rotational" ? CameraShakePacket::ShakeType::Rotational
                                                 : CameraShakePacket::ShakeType::Positional;
        packet.mShakeAction = CameraShakePacket::ShakeAction::Add;
        if (player != nullptr)
            api->host().sendPacketTo(player->getNetworkIdentifier(), packet);
        return JS_UNDEFINED;
    }

    JSValue cameraStopShaking(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player == nullptr)
            return JS_UNDEFINED;
        CameraShakePacket packet;
        packet.mIntensity = 0.0f;
        packet.mDuration = 0.0f;
        packet.mShakeType = CameraShakePacket::ShakeType::Positional;
        packet.mShakeAction = CameraShakePacket::ShakeAction::Stop;
        ScriptApi::fromContext(ctx)->host().sendPacketTo(player->getNetworkIdentifier(), packet);
        return JS_UNDEFINED;
    }

    JSValue cameraClear(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = partOwner(ctx, thisVal);
        if (player != nullptr)
            ScriptApi::fromContext(ctx)->host().clearPlayerCamera(*player);
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
        if (const ScriptBlockPart *block = api->native<ScriptBlockPart>(thisVal))
            return string(ctx, block->mComponentId);
        return JS_UNDEFINED;
    }

    JSValue componentGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (api->native<ScriptEntityPart>(thisVal) != nullptr)
            return JS_NewBool(ctx, componentEntity(ctx, thisVal) != nullptr);
        if (api->native<ScriptPlayerPart>(thisVal) != nullptr)
            return JS_NewBool(ctx, partOwner(ctx, thisVal) != nullptr);
        if (const ScriptBlockPart *block = api->native<ScriptBlockPart>(thisVal))
            return JS_NewBool(ctx, api->resolveLevel(block->mBlock.mWorldId, block->mBlock.mDimension)
                                                   .peekBlockPtr(block->mBlock.mX, block->mBlock.mY, block->mBlock.mZ)
                                           != nullptr);
        return JS_TRUE;
    }
}

std::string ScriptApi::effectTypeId(MobEffectId id) {
    return getMobEffectName(id);
}

JSValue ScriptApi::makeEffect(Actor &actor, MobEffectId id) {
    ScriptEffectRef *ref = new ScriptEffectRef();
    JSValue entity = makeEntity(actor);
    const ScriptEntityRef *entityRef = native<ScriptEntityRef>(entity);
    if (entityRef != nullptr)
        ref->mEntity = *entityRef;
    JS_FreeValue(mContext, entity);
    ref->mEffectId = (int32_t) id;
    return makeInstance("Effect", ref);
}

std::string ScriptApi::damageCauseName(const ActorDamageSource &source) {
    if (source.mProjectile)
        return "projectile";

    std::string cause = source.mCause;
    if (cause.empty()) {
        if (source.mDeathMessageKey == "death.attack.player" || source.mDeathMessageKey == "death.attack.mob")
            return "entityAttack";
        for (const std::string &name: DamageCause::getNames()) {
            if (DamageCause::matches(name, source.mDeathMessageKey)) {
                cause = name;
                break;
            }
        }
    }
    if (cause.empty())
        return source.mAttacker != nullptr ? "entityAttack" : "none";

    // the game's causes are snake_case, the script enum camelCase
    std::string camel;
    bool upper = false;
    for (const char character: cause) {
        if (character == '_') {
            upper = true;
            continue;
        }
        camel += upper && character >= 'a' && character <= 'z' ? (char) (character - 'a' + 'A') : character;
        upper = false;
    }
    return camel;
}

JSValue ScriptApi::makeDamageSource(const ActorDamageSource &source) {
    JSValue object = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, object, "cause", string(mContext, damageCauseName(source)));
    if (source.mAttacker != nullptr)
        JS_SetPropertyStr(mContext, object, "damagingEntity", makeEntity(*source.mAttacker));
    if (source.mProjectile && source.mDamager != nullptr)
        JS_SetPropertyStr(mContext, object, "damagingProjectile", makeEntity(*source.mDamager));
    return object;
}

JSValue ScriptApi::makeHealSource(const char *cause) {
    ScriptNamedRef *ref = new ScriptNamedRef();
    ref->mName = cause;
    return makeInstance("EntityHealSource", ref);
}

namespace {
    JSValue modifiersGetter(JSContext *ctx, JSValueConst, int, JSValueConst *, int, JSValue *data) {
        JSValue array = JS_NewArray(ctx);
        JSValue modifier = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, modifier, "addedComponentGroups", JS_DupValue(ctx, data[0]));
        JS_SetPropertyStr(ctx, modifier, "removedComponentGroups", JS_DupValue(ctx, data[1]));
        JS_SetPropertyUint32(ctx, array, 0, modifier);
        return array;
    }

    JSValue stringArray(JSContext *ctx, const std::vector<std::string> &values) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &value: values)
            JS_SetPropertyUint32(ctx, array, index++, string(ctx, value));
        return array;
    }

    // 1.x names the game modes in lowercase, 2.0 capitalised
    const char *const GAME_MODES_1[] = {"survival", "creative", "adventure", "spectator"};
    const char *const GAME_MODES_2[] = {"Survival", "Creative", "Adventure", "Spectator"};
}

JSValue ScriptApi::makeModifiersGetter(const std::vector<std::string> &added, const std::vector<std::string> &removed) {
    JSValue data[2] = {stringArray(mContext, added), stringArray(mContext, removed)};
    JSValue getter = JS_NewCFunctionData(mContext, modifiersGetter, 0, 0, 2, data);
    JS_FreeValue(mContext, data[0]);
    JS_FreeValue(mContext, data[1]);
    return getter;
}

const char *ScriptApi::gameModeName(int32_t gameType) const {
    const char *const *names = isStable1() ? GAME_MODES_1 : GAME_MODES_2;
    // GameType::Spectator is 6 on the wire
    const int32_t index = gameType == 6 ? 3 : gameType;
    return index >= 0 && index <= 3 ? names[index] : names[0];
}

int32_t ScriptApi::parseGameMode(const std::string &name) const {
    const char *const *names = isStable1() ? GAME_MODES_1 : GAME_MODES_2;
    for (int32_t i = 0; i < 4; ++i) {
        if (name == names[i])
            return i == 3 ? 6 : i;
    }
    return -1;
}

bool ScriptRayQueries::blockPassesFilter(JSContext *ctx, const BlockState &state, JSValueConst filter) {
    return blockStatePassesFilter(ctx, state, filter);
}

JSValue ScriptRayQueries::blockFromRay(JSContext *ctx, Level &level, const Vector3f &origin, const Vector3f &direction,
                                       JSValueConst options) {
    ScriptApi *api = ScriptApi::fromContext(ctx);
    ScriptRaycast::BlockOptions ray;
    if (!readRayDistance(ctx, options, ray.mMaxDistance))
        return JS_UNDEFINED;
    ray.mIncludeLiquids = readFlag(ctx, options, "includeLiquidBlocks");
    ray.mIncludePassable = readFlag(ctx, options, "includePassableBlocks");
    ray.mAccepts = [ctx, options](const BlockState &state) { return blockStatePassesFilter(ctx, state, options); };

    ScriptRaycast::BlockHit hit;
    if (!ScriptRaycast::castBlocks(level, origin, direction, ray, hit))
        return JS_UNDEFINED;

    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "block", api->makeBlock(level, hit.mBlock.x, hit.mBlock.y, hit.mBlock.z));
    JS_SetPropertyStr(ctx, result, "face", JS_NewString(ctx, hit.mFace));
    JS_SetPropertyStr(ctx, result, "faceLocation",
                      vector3(ctx, hit.mFaceLocation.x, hit.mFaceLocation.y, hit.mFaceLocation.z));
    return result;
}

JSValue ScriptRayQueries::entitiesFromRay(JSContext *ctx, Level &level, const Vector3f &origin,
                                          const Vector3f &direction, JSValueConst options, const Actor *except) {
    ScriptApi *api = ScriptApi::fromContext(ctx);
    JSValue array = JS_NewArray(ctx);
    float distance = 0.0f;
    if (!readRayDistance(ctx, options, distance))
        return array;

    ServerNetworkHandler &host = api->host();

    // blocks stop the ray unless ignoreBlockCollision
    if (!readFlag(ctx, options, "ignoreBlockCollision")) {
        ScriptRaycast::BlockOptions ray;
        ray.mMaxDistance = distance;
        ray.mIncludeLiquids = readFlag(ctx, options, "includeLiquidBlocks");
        ray.mIncludePassable = readFlag(ctx, options, "includePassableBlocks");
        ScriptRaycast::BlockHit hit;
        if (ScriptRaycast::castBlocks(level, origin, direction, ray, hit))
            distance = std::min(distance, hit.mDistance);
    }

    std::vector<std::pair<float, JSValue>> hits;
    const auto consider = [&](Actor &candidate, JSValue value) {
        EntityTarget other;
        other.mApi = api;
        other.mRef = api->native<ScriptEntityRef>(value);
        other.mActor = &candidate;
        other.mPlayer = dynamic_cast<ServerPlayer *>(&candidate);
        other.mServerActor = dynamic_cast<ServerActor *>(&candidate);
        const ActorSize size = sizeOf(other);
        const Vector3f &position = candidate.getPosition();
        const float half = size.mWidth / 2.0f;
        const float at = ScriptRaycast::intersectBox(
                origin, direction, Vector3f(position.x - half, position.y, position.z - half),
                Vector3f(position.x + half, position.y + size.mHeight, position.z + half));
        if (at < 0.0f || at > distance || other.mRef == nullptr || !matchesQuery(ctx, other, options)) {
            JS_FreeValue(ctx, value);
            return;
        }
        hits.emplace_back(at, value);
    };
    for (auto &entry: host.getActors()) {
        ServerActor &actor = *entry.second;
        if (&actor != except && actor.isAlive() && actor.isIn(level))
            consider(actor, api->makeActor(actor));
    }
    for (auto &entry: host.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (&player != except && player.isSpawned() && player.isIn(level))
            consider(player, api->makePlayer(player));
    }

    std::stable_sort(hits.begin(), hits.end(),
                     [](const std::pair<float, JSValue> &left, const std::pair<float, JSValue> &right) {
                         return left.first < right.first;
                     });
    uint32_t index = 0;
    for (const auto &found: hits) {
        JSValue entry = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, entry, "distance", JS_NewFloat64(ctx, found.first));
        JS_SetPropertyStr(ctx, entry, "entity", found.second);
        JS_SetPropertyUint32(ctx, array, index++, entry);
    }
    return array;
}

void ScriptApi::_bindEntity() {
    JSValue entity = defineClass("Entity");
    property(entity, "Entity", "typeId", entityGetTypeId);
    property(entity, "Entity", "id", entityGetId);
    property(entity, "Entity", "isValid", entityGetIsValid);
    property(entity, "Entity", "location", entityGetLocation);
    property(entity, "Entity", "dimension", entityGetDimension);
    property(entity, "Entity", "nameTag", entityGetNameTag, entitySetNameTag);
    property(entity, "Entity", "nameplateDepthTested", entityGetNameplateDepthTested, entitySetNameplateDepthTested);
    property(entity, "Entity", "nameplateRenderDistance", entityGetNameplateRenderDistance,
             entitySetNameplateRenderDistance);
    property(entity, "Entity", "localizationKey", entityGetLocalizationKey);
    property(entity, "Entity", "target", entityGetTarget);
    propertyMagic(entity, "Entity", "isSneaking", entityGetFlag, (int) EntityState::Sneaking,
                  [](JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int) {
                      return entitySetSneaking(ctx, thisVal, argc, argv);
                  });
    propertyMagic(entity, "Entity", "isSprinting", entityGetFlag, (int) EntityState::Sprinting);
    propertyMagic(entity, "Entity", "isClimbing", entityGetFlag, (int) EntityState::Climbing);
    propertyMagic(entity, "Entity", "isFalling", entityGetFlag, (int) EntityState::Falling);
    propertyMagic(entity, "Entity", "isInWater", entityGetFlag, (int) EntityState::InWater);
    propertyMagic(entity, "Entity", "isOnGround", entityGetFlag, (int) EntityState::OnGround);
    propertyMagic(entity, "Entity", "isSleeping", entityGetFlag, (int) EntityState::Sleeping);
    propertyMagic(entity, "Entity", "isSwimming", entityGetFlag, (int) EntityState::Swimming);

    method(entity, "Entity", "remove", entityRemove, 0);
    method(entity, "Entity", "teleport", entityTeleport, 2);
    method(entity, "Entity", "tryTeleport", entityTryTeleport, 2);
    method(entity, "Entity", "lookAt", entityLookAt, 1);
    method(entity, "Entity", "triggerEvent", entityTriggerEvent, 1);
    method(entity, "Entity", "getEffects", entityGetEffects, 0);
    method(entity, "Entity", "getAABB", entityGetAABB, 0);
    method(entity, "Entity", "getBlockFromViewDirection", entityGetBlockFromViewDirection, 1);
    method(entity, "Entity", "getEntitiesFromViewDirection", entityGetEntitiesFromViewDirection, 1);
    method(entity, "Entity", "getBlockStandingOn", entityGetBlockStandingOn, 1);
    method(entity, "Entity", "getAllBlocksStandingOn", entityGetAllBlocksStandingOn, 1);
    method(entity, "Entity", "resetProperty", entityResetProperty, 1);
    method(entity, "Entity", "setDynamicProperties", entitySetDynamicProperties, 1);
    method(entity, "Entity", "getDynamicPropertyTotalByteCount", entityGetDynamicPropertyTotalByteCount, 0);
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

    JSValue effect = defineClass("Effect");
    propertyMagic(effect, "Effect", "typeId", effectGetField, (int) EffectField::TypeId);
    propertyMagic(effect, "Effect", "amplifier", effectGetField, (int) EffectField::Amplifier);
    propertyMagic(effect, "Effect", "duration", effectGetField, (int) EffectField::Duration);
    propertyMagic(effect, "Effect", "displayName", effectGetField, (int) EffectField::DisplayName);
    property(effect, "Effect", "isValid", effectGetIsValid);

    JSValue effectType = defineClass("EffectType");
    method(effectType, "EffectType", "getName", namedGetName, 0);
    defineClass("EffectTypes");
    JSValue effectTypes = constructorOf("EffectTypes");
    method(effectTypes, "EffectTypes", "static get", effectTypesGet, 1);
    method(effectTypes, "EffectTypes", "static getAll", effectTypesGetAll, 0);

    JSValue entityType = defineClass("EntityType");
    property(entityType, "EntityType", "id", namedGetName);
    property(entityType, "EntityType", "localizationKey", entityTypeGetLocalizationKey);
    defineClass("EntityTypes");
    JSValue entityTypes = constructorOf("EntityTypes");
    method(entityTypes, "EntityTypes", "static get", entityTypesGet, 1);
    method(entityTypes, "EntityTypes", "static getAll", entityTypesGetAll, 0);

    JSValue healSource = defineClass("EntityHealSource");
    property(healSource, "EntityHealSource", "cause", namedGetName);

    JSValue input = defineClass("InputInfo");
    property(input, "InputInfo", "lastInputModeUsed", inputGetLastMode);
    method(input, "InputInfo", "getButtonState", inputGetButtonState, 1);
    method(input, "InputInfo", "getMovementVector", inputGetMovementVector, 0);
}

void ScriptApi::_bindPlayer() {
    JSValue player = defineClass("Player");
    property(player, "Player", "name", playerGetName);
    property(player, "Player", "persistentId", playerGetPersistentId);
    property(player, "Player", "level", playerGetLevel);
    property(player, "Player", "selectedSlotIndex", playerGetSelectedSlot, playerSetSelectedSlot);
    propertyMagic(player, "Player", "isFlying", playerGetState, (int) PlayerState::Flying);
    propertyMagic(player, "Player", "isGliding", playerGetState, (int) PlayerState::Gliding);
    propertyMagic(player, "Player", "isJumping", playerGetState, (int) PlayerState::Jumping);
    propertyMagic(player, "Player", "isEmoting", playerGetState, (int) PlayerState::Emoting);
    propertyMagic(player, "Player", "xpEarnedAtCurrentLevel", playerGetExperience,
                  (int) PlayerExperience::EarnedAtLevel);
    propertyMagic(player, "Player", "totalXpNeededForNextLevel", playerGetExperience,
                  (int) PlayerExperience::NeededForNext);
    property(player, "Player", "commandPermissionLevel", playerGetCommandPermissionLevel,
             playerSetCommandPermissionLevel);
    propertyMagic(player, "Player", "chatNamePrefix", playerGetChat, (int) ChatField::NamePrefix, playerSetChat);
    propertyMagic(player, "Player", "chatNameSuffix", playerGetChat, (int) ChatField::NameSuffix, playerSetChat);
    propertyMagic(player, "Player", "chatMessagePrefix", playerGetChat, (int) ChatField::MessagePrefix, playerSetChat);
    property(player, "Player", "chatDisplayName", playerGetChatDisplayName);
    methodMagic(player, "Player", "stopSound", playerStopSound, 1, 0);
    methodMagic(player, "Player", "stopAllSounds", playerStopSound, 0, 1);
    method(player, "Player", "postClientMessage", playerPostClientMessage, 2);
    method(player, "Player", "eatItem", playerEatItem, 1);
    method(player, "Player", "getPing", playerGetPing, 0);
    property(player, "Player", "playerPermissionLevel", playerGetPlayerPermissionLevel);
    property(player, "Player", "inputInfo", playerGetInputInfo);
    methodMagic(player, "Player", "getTotalXp", playerGetExperience, 0, (int) PlayerExperience::Total);
    method(player, "Player", "getSpawnPoint", playerGetSpawnPoint, 0);
    method(player, "Player", "setSpawnPoint", playerSetSpawnPoint, 1);
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
    method(screen, "ScreenDisplay", "updateSubtitle", screenUpdateSubtitle, 1);
    method(screen, "ScreenDisplay", "setHudVisibility", screenSetHudVisibility, 2);
    method(screen, "ScreenDisplay", "hideAllExcept", screenHideAllExcept, 1);
    method(screen, "ScreenDisplay", "resetHudElementsVisibility", screenResetHudElementsVisibility, 0);
    method(screen, "ScreenDisplay", "resetHudElements", screenResetHudElementsVisibility, 0);
    method(screen, "ScreenDisplay", "isForcedHidden", screenIsForcedHidden, 1);
    method(screen, "ScreenDisplay", "getHiddenHudElements", screenGetHiddenHudElements, 0);

    JSValue camera = defineClass("Camera");
    property(camera, "Camera", "isValid", partGetIsValid);
    method(camera, "Camera", "clear", cameraClear, 0);
    method(camera, "Camera", "setCamera", cameraSetCamera, 2);
    method(camera, "Camera", "setDefaultCamera", cameraSetDefaultCamera, 2);
    method(camera, "Camera", "fade", cameraFade, 1);
    method(camera, "Camera", "setFov", cameraSetFov, 1);
    method(camera, "Camera", "attachToEntity", cameraAttachToEntity, 1);
    method(camera, "Camera", "addShake", cameraAddShake, 1);
    method(camera, "Camera", "stopShaking", cameraStopShaking, 0);

    JSValue permissions = defineClass("PlayerInputPermissions");
    method(permissions, "PlayerInputPermissions", "setPermissionCategory", inputPermissionSet, 2);
    // the 1.x properties: InputPermissionCategory.Camera and Movement
    property(permissions, "PlayerInputPermissions", "cameraEnabled", inputCategoryGet<1>, inputCategorySet<1>);
    property(permissions, "PlayerInputPermissions", "movementEnabled", inputCategoryGet<2>, inputCategorySet<2>);
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
}
