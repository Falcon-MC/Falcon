#include "Scripting/Binding/ScriptApi.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LocatorBarPacket.h"
#include "World/Actor/Actor.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>
#include <limits>
#include <random>

using namespace ScriptValues;

namespace {
    // ScriptLocatorBar's waypoint group holds 0x100 (sub_14D519BB0, 26.60)
    const int32_t MAX_WAYPOINTS = 256;
    // the typings cap textureBoundsList
    const size_t MAX_TEXTURE_BOUNDS = 16;

    // LocatorBarWaypointPayload update bits (sub_140B87D00, 26.60)
    const int32_t UPDATE_POSITION = 0x01;
    const int32_t UPDATE_VISIBLE = 0x02;
    const int32_t UPDATE_TEXTURE = 0x04;
    const int32_t UPDATE_ICON_SIZE = 0x08;
    const int32_t UPDATE_COLOR = 0x10;

    // WaypointTexture values as VanillaWaypointManagerConstants::ImageType 2 to 5: the dot texture and its icon
    // scale (sub_14C2C9150, sub_142990370, qword_14F47FE40, 26.60)
    struct BuiltInTexture {
        const char *mValue;
        const char *mPath;
        float mScale;
    };

    const BuiltInTexture BUILT_IN_TEXTURES[] = {
            {"minecraft:square", "textures/ui/locator_bar_dot_0", 1.0f},
            {"minecraft:circle", "textures/ui/locator_bar_dot_1", 1.0f},
            {"minecraft:small_square", "textures/ui/locator_bar_dot_2", 0.6f},
            {"minecraft:small_star", "textures/ui/locator_bar_dot_3", 0.2f}};

    const BuiltInTexture *findBuiltIn(const std::string &value) {
        for (const BuiltInTexture &texture: BUILT_IN_TEXTURES) {
            if (value == texture.mValue)
                return &texture;
        }
        return nullptr;
    }

    std::shared_ptr<ScriptWaypointState> stateOf(JSContext *ctx, JSValueConst value) {
        const ScriptWaypointRef *ref = ScriptApi::fromContext(ctx)->native<ScriptWaypointRef>(value);
        return ref == nullptr ? nullptr : ref->mState;
    }

    JSValue property(JSContext *ctx, JSValueConst object, const char *key) {
        return JS_IsObject(object) ? JS_GetPropertyStr(ctx, object, key) : JS_UNDEFINED;
    }

    bool readNumber(JSContext *ctx, JSValueConst value, double &out) {
        return JS_IsNumber(value) && JS_ToFloat64(ctx, &out, value) == 0 && std::isfinite(out);
    }

    bool readNumberProperty(JSContext *ctx, JSValueConst object, const char *key, double &out) {
        JSValue value = property(ctx, object, key);
        const bool ok = readNumber(ctx, value, out);
        JS_FreeValue(ctx, value);
        return ok;
    }

    // undefined keeps the default, as every rule defaults to true
    bool readRule(JSContext *ctx, JSValueConst rules, const char *key, bool &out) {
        JSValue value = property(ctx, rules, key);
        bool ok = true;
        if (!JS_IsUndefined(value)) {
            ok = JS_IsBool(value);
            out = ok && JS_ToBool(ctx, value) == 1;
        }
        JS_FreeValue(ctx, value);
        return ok;
    }

    bool readEntityRules(JSContext *ctx, JSValueConst rules, ScriptWaypointState &state) {
        if (!JS_IsObject(rules))
            return false;
        return readRule(ctx, rules, "showDead", state.mShowDead) && readRule(ctx, rules, "showInvisible", state.mShowInvisible)
               && readRule(ctx, rules, "showSneaking", state.mShowSneaking);
    }

    bool readPlayerRules(JSContext *ctx, JSValueConst rules, ScriptWaypointState &state) {
        return readEntityRules(ctx, rules, state) && readRule(ctx, rules, "showHidden", state.mShowHidden)
               && readRule(ctx, rules, "showSpectator", state.mShowSpectator)
               && readRule(ctx, rules, "showSpectatorToSpectator", state.mShowSpectatorToSpectator);
    }

    // undefined clears the colour; channels in [0, 1] as RGB holds them
    bool readColor(JSContext *ctx, JSValueConst value, ScriptWaypointState &state) {
        if (JS_IsUndefined(value)) {
            state.mHasColor = false;
            return true;
        }
        double red = 0.0, green = 0.0, blue = 0.0;
        if (!readNumberProperty(ctx, value, "red", red) || !readNumberProperty(ctx, value, "green", green)
            || !readNumberProperty(ctx, value, "blue", blue))
            return false;
        state.mHasColor = true;
        state.mRed = red;
        state.mGreen = green;
        state.mBlue = blue;
        return true;
    }

    enum class SelectorResult { Ok, Conversion, Invalid };

    SelectorResult readTextureSelector(JSContext *ctx, JSValueConst value,
                                       std::vector<ScriptWaypointTexture> &textures) {
        JSValue list = property(ctx, value, "textureBoundsList");
        if (!JS_IsArray(list)) {
            JS_FreeValue(ctx, list);
            return SelectorResult::Conversion;
        }
        JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
        uint32_t length = 0;
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);

        SelectorResult result = length > MAX_TEXTURE_BOUNDS ? SelectorResult::Invalid : SelectorResult::Ok;
        std::vector<ScriptWaypointTexture> read;
        for (uint32_t index = 0; result == SelectorResult::Ok && index < length; ++index) {
            JSValue bounds = JS_GetPropertyUint32(ctx, list, index);
            ScriptWaypointTexture texture;
            JSValue upper = property(ctx, bounds, "upperBound");
            JSValue entry = property(ctx, bounds, "texture");
            if (!readNumberProperty(ctx, bounds, "lowerBound", texture.mLowerBound)) {
                result = SelectorResult::Conversion;
            } else if (!JS_IsUndefined(upper)) {
                texture.mHasUpperBound = readNumber(ctx, upper, texture.mUpperBound);
                if (!texture.mHasUpperBound)
                    result = SelectorResult::Conversion;
            }
            if (result == SelectorResult::Ok && JS_IsString(entry)) {
                texture.mTexture = toString(ctx, entry);
                if (findBuiltIn(texture.mTexture) == nullptr)
                    result = SelectorResult::Conversion;
            } else if (result == SelectorResult::Ok && JS_IsObject(entry)) {
                JSValue path = JS_GetPropertyStr(ctx, entry, "path");
                texture.mCustom = true;
                if (!JS_IsString(path) || !readNumberProperty(ctx, entry, "iconWidth", texture.mIconWidth)
                    || !readNumberProperty(ctx, entry, "iconHeight", texture.mIconHeight))
                    result = SelectorResult::Conversion;
                else
                    texture.mTexture = toString(ctx, path);
                JS_FreeValue(ctx, path);
            } else if (result == SelectorResult::Ok) {
                result = SelectorResult::Conversion;
            }
            if (result == SelectorResult::Ok
                && (texture.mLowerBound < 0.0 || (texture.mHasUpperBound && texture.mUpperBound < 0.0)))
                result = SelectorResult::Invalid;
            JS_FreeValue(ctx, upper);
            JS_FreeValue(ctx, entry);
            JS_FreeValue(ctx, bounds);
            read.push_back(texture);
        }
        JS_FreeValue(ctx, list);
        if (result == SelectorResult::Ok)
            textures = std::move(read);
        return result;
    }

    JSValue throwSelector(JSContext *ctx, SelectorResult result) {
        if (result == SelectorResult::Conversion)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return ScriptApi::fromContext(ctx)->throwError("InvalidWaypointTextureSelectorError", "");
    }

    JSValue throwLocatorBar(JSContext *ctx, const char *reason) {
        JSContext *context = ctx;
        JSValue fields = JS_NewObject(context);
        JS_SetPropertyStr(context, fields, "reason", JS_NewString(context, reason));
        return ScriptApi::fromContext(ctx)->throwError("LocatorBarError", "", fields);
    }

    Actor *trackedActor(ScriptApi &api, const ScriptWaypointState &state) {
        return state.mKind == ScriptWaypointState::Kind::Location ? nullptr : api.resolveEntity(state.mEntity);
    }

    bool isValid(ScriptApi &api, const ScriptWaypointState &state) {
        return !state.mRemoved && (state.mKind == ScriptWaypointState::Kind::Location || trackedActor(api, state) != nullptr);
    }

    // the waypoint object, the constructor arguments already read into state
    JSValue makeWaypoint(JSContext *ctx, const char *className, std::shared_ptr<ScriptWaypointState> state) {
        ScriptWaypointRef *ref = new ScriptWaypointRef();
        ref->mState = std::move(state);
        return ScriptApi::fromContext(ctx)->makeInstance(className, ref);
    }

    bool readDimensionLocation(JSContext *ctx, JSValueConst value, ScriptWaypointState &state) {
        JSValue dimension = property(ctx, value, "dimension");
        const ScriptDimensionRef *ref = ScriptApi::fromContext(ctx)->native<ScriptDimensionRef>(dimension);
        JS_FreeValue(ctx, dimension);
        if (ref == nullptr || !readNumberProperty(ctx, value, "x", state.mX) || !readNumberProperty(ctx, value, "y", state.mY)
            || !readNumberProperty(ctx, value, "z", state.mZ))
            return false;
        state.mDimension = ref->mDimension;
        state.mWorldId = ref->mWorldId;
        return true;
    }

    JSValue locationWaypointConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        std::shared_ptr<ScriptWaypointState> state = std::make_shared<ScriptWaypointState>();
        state->mKind = ScriptWaypointState::Kind::Location;
        if (argc < 2 || !readDimensionLocation(ctx, argv[0], *state)
            || !readColor(ctx, argc >= 3 ? argv[2] : JS_UNDEFINED, *state))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const SelectorResult selector = readTextureSelector(ctx, argv[1], state->mTextures);
        if (selector != SelectorResult::Ok)
            return throwSelector(ctx, selector);
        return makeWaypoint(ctx, "LocationWaypoint", std::move(state));
    }

    JSValue entityWaypointCommon(JSContext *ctx, int argc, JSValueConst *argv, bool player) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptEntityRef *entity = argc >= 1 ? api->native<ScriptEntityRef>(argv[0]) : nullptr;
        std::shared_ptr<ScriptWaypointState> state = std::make_shared<ScriptWaypointState>();
        state->mKind = player ? ScriptWaypointState::Kind::Player : ScriptWaypointState::Kind::Entity;
        if (entity == nullptr || (player && entity->mKind != ScriptEntityRef::Kind::Player) || argc < 3
            || !(player ? readPlayerRules(ctx, argv[2], *state) : readEntityRules(ctx, argv[2], *state))
            || !readColor(ctx, argc >= 4 ? argv[3] : JS_UNDEFINED, *state))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        state->mEntity = *entity;
        if (api->resolveEntity(state->mEntity) == nullptr)
            return api->throwError("InvalidWaypointError", "");
        const SelectorResult selector = readTextureSelector(ctx, argv[1], state->mTextures);
        if (selector != SelectorResult::Ok)
            return throwSelector(ctx, selector);
        return makeWaypoint(ctx, player ? "PlayerWaypoint" : "EntityWaypoint", std::move(state));
    }

    JSValue entityWaypointConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        return entityWaypointCommon(ctx, argc, argv, false);
    }

    JSValue playerWaypointConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        return entityWaypointCommon(ctx, argc, argv, true);
    }

    JSValue waypointGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        return JS_NewBool(ctx, state != nullptr && isValid(*ScriptApi::fromContext(ctx), *state));
    }

    JSValue waypointGetIsEnabled(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        return JS_NewBool(ctx, state != nullptr && state->mEnabled);
    }

    JSValue waypointSetIsEnabled(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        if (argc < 1 || !JS_IsBool(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (state != nullptr)
            state->mEnabled = JS_ToBool(ctx, argv[0]) == 1;
        return JS_UNDEFINED;
    }

    JSValue waypointGetColor(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        if (state == nullptr || !state->mHasColor)
            return JS_UNDEFINED;
        JSValue color = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, color, "red", JS_NewFloat64(ctx, state->mRed));
        JS_SetPropertyStr(ctx, color, "green", JS_NewFloat64(ctx, state->mGreen));
        JS_SetPropertyStr(ctx, color, "blue", JS_NewFloat64(ctx, state->mBlue));
        return color;
    }

    JSValue waypointSetColor(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        ScriptWaypointState read = state == nullptr ? ScriptWaypointState() : *state;
        if (!readColor(ctx, argc >= 1 ? argv[0] : JS_UNDEFINED, read))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (state != nullptr) {
            state->mHasColor = read.mHasColor;
            state->mRed = read.mRed;
            state->mGreen = read.mGreen;
            state->mBlue = read.mBlue;
        }
        return JS_UNDEFINED;
    }

    JSValue waypointGetTextureSelector(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        JSValue list = JS_NewArray(ctx);
        for (uint32_t index = 0; state != nullptr && index < state->mTextures.size(); ++index) {
            const ScriptWaypointTexture &texture = state->mTextures[index];
            JSValue bounds = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, bounds, "lowerBound", JS_NewFloat64(ctx, texture.mLowerBound));
            if (texture.mHasUpperBound)
                JS_SetPropertyStr(ctx, bounds, "upperBound", JS_NewFloat64(ctx, texture.mUpperBound));
            if (texture.mCustom) {
                JSValue custom = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, custom, "path", string(ctx, texture.mTexture));
                JS_SetPropertyStr(ctx, custom, "iconWidth", JS_NewFloat64(ctx, texture.mIconWidth));
                JS_SetPropertyStr(ctx, custom, "iconHeight", JS_NewFloat64(ctx, texture.mIconHeight));
                JS_SetPropertyStr(ctx, bounds, "texture", custom);
            } else {
                JS_SetPropertyStr(ctx, bounds, "texture", string(ctx, texture.mTexture));
            }
            JS_SetPropertyUint32(ctx, list, index, bounds);
        }
        JSValue selector = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, selector, "textureBoundsList", list);
        return selector;
    }

    JSValue waypointSetTextureSelector(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        std::vector<ScriptWaypointTexture> textures;
        const SelectorResult selector = readTextureSelector(ctx, argc >= 1 ? argv[0] : JS_UNDEFINED, textures);
        if (selector != SelectorResult::Ok)
            return throwSelector(ctx, selector);
        if (state != nullptr)
            state->mTextures = std::move(textures);
        return JS_UNDEFINED;
    }

    JSValue waypointGetDimensionLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        if (state == nullptr || !isValid(*api, *state))
            return api->throwError("InvalidWaypointError", "");

        JSValue location = JS_NewObject(ctx);
        Actor *actor = trackedActor(*api, *state);
        if (actor != nullptr) {
            const Vector3f &position = actor->getPosition();
            JS_SetPropertyStr(ctx, location, "dimension", api->makeDimension(api->host().getLevelFor(*actor)));
            JS_SetPropertyStr(ctx, location, "x", JS_NewFloat64(ctx, position.x));
            JS_SetPropertyStr(ctx, location, "y", JS_NewFloat64(ctx, position.y));
            JS_SetPropertyStr(ctx, location, "z", JS_NewFloat64(ctx, position.z));
            return location;
        }
        JS_SetPropertyStr(ctx, location, "dimension",
                          api->makeDimension(api->resolveLevel(state->mWorldId, state->mDimension)));
        JS_SetPropertyStr(ctx, location, "x", JS_NewFloat64(ctx, state->mX));
        JS_SetPropertyStr(ctx, location, "y", JS_NewFloat64(ctx, state->mY));
        JS_SetPropertyStr(ctx, location, "z", JS_NewFloat64(ctx, state->mZ));
        return location;
    }

    JSValue locationWaypointSetDimensionLocation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        ScriptWaypointState read;
        if (argc < 1 || !readDimensionLocation(ctx, argv[0], read))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (state != nullptr) {
            state->mDimension = read.mDimension;
            state->mWorldId = read.mWorldId;
            state->mX = read.mX;
            state->mY = read.mY;
            state->mZ = read.mZ;
        }
        return JS_UNDEFINED;
    }

    // Waypoint.remove: out of every locator bar on the next tick
    JSValue waypointRemove(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        if (state != nullptr)
            state->mRemoved = true;
        return JS_UNDEFINED;
    }

    JSValue entityWaypointGetEntity(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        Actor *actor = state == nullptr ? nullptr : trackedActor(*api, *state);
        if (actor == nullptr)
            return api->throwError("InvalidWaypointError", "");
        return api->makeEntity(*actor);
    }

    JSValue rulesObject(JSContext *ctx, const ScriptWaypointState &state, bool player) {
        JSValue rules = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, rules, "showDead", JS_NewBool(ctx, state.mShowDead));
        JS_SetPropertyStr(ctx, rules, "showInvisible", JS_NewBool(ctx, state.mShowInvisible));
        JS_SetPropertyStr(ctx, rules, "showSneaking", JS_NewBool(ctx, state.mShowSneaking));
        if (player) {
            JS_SetPropertyStr(ctx, rules, "showHidden", JS_NewBool(ctx, state.mShowHidden));
            JS_SetPropertyStr(ctx, rules, "showSpectator", JS_NewBool(ctx, state.mShowSpectator));
            JS_SetPropertyStr(ctx, rules, "showSpectatorToSpectator", JS_NewBool(ctx, state.mShowSpectatorToSpectator));
        }
        return rules;
    }

    JSValue entityWaypointGetEntityRules(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        return state == nullptr ? JS_UNDEFINED : rulesObject(ctx, *state, false);
    }

    JSValue playerWaypointGetPlayerRules(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const std::shared_ptr<ScriptWaypointState> state = stateOf(ctx, thisVal);
        return state == nullptr ? JS_UNDEFINED : rulesObject(ctx, *state, true);
    }

    std::vector<ScriptApi::LocatorEntry> *barOf(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptPlayerPart *part = api->native<ScriptPlayerPart>(thisVal);
        if (part == nullptr || api->resolvePlayerByHandle(part->mHandle) == nullptr)
            return nullptr;
        return &api->locatorBarOf(part->mHandle);
    }

    std::vector<ScriptApi::LocatorEntry>::iterator findEntry(std::vector<ScriptApi::LocatorEntry> &bar,
                                                            const std::shared_ptr<ScriptWaypointState> &state) {
        for (auto it = bar.begin(); it != bar.end(); ++it) {
            if (it->mState == state && !it->mState->mRemoved)
                return it;
        }
        return bar.end();
    }

    size_t liveCount(const std::vector<ScriptApi::LocatorEntry> &bar) {
        size_t count = 0;
        for (const ScriptApi::LocatorEntry &entry: bar) {
            if (!entry.mState->mRemoved)
                ++count;
        }
        return count;
    }

    // ScriptLocatorBar::addWaypoint (sub_14D59E890, 26.60)
    JSValue locatorAddWaypoint(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::shared_ptr<ScriptWaypointState> state = argc >= 1 ? stateOf(ctx, argv[0]) : nullptr;
        if (state == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        std::vector<ScriptApi::LocatorEntry> *bar = barOf(ctx, thisVal);
        if (bar == nullptr)
            return api->throwError("EngineError", "");
        if (!isValid(*api, *state))
            return api->throwError("InvalidWaypointError", "");
        if (findEntry(*bar, state) != bar->end())
            return throwLocatorBar(ctx, "WaypointAlreadyExists");
        if (liveCount(*bar) >= (size_t) MAX_WAYPOINTS)
            return throwLocatorBar(ctx, "WaypointLimitExceeded");

        ScriptApi::LocatorEntry entry;
        entry.mState = state;
        entry.mObject = JS_DupValue(ctx, argv[0]);
        entry.mHandle = api->nextWaypointHandle();
        bar->push_back(std::move(entry));
        return JS_UNDEFINED;
    }

    JSValue locatorRemoveWaypoint(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::shared_ptr<ScriptWaypointState> state = argc >= 1 ? stateOf(ctx, argv[0]) : nullptr;
        if (state == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        std::vector<ScriptApi::LocatorEntry> *bar = barOf(ctx, thisVal);
        if (bar == nullptr)
            return api->throwError("EngineError", "");
        const auto it = findEntry(*bar, state);
        if (it == bar->end())
            return throwLocatorBar(ctx, "WaypointNotFound");
        it->mState = std::make_shared<ScriptWaypointState>(*state);
        it->mState->mRemoved = true;
        return JS_UNDEFINED;
    }

    JSValue locatorRemoveAllWaypoints(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::vector<ScriptApi::LocatorEntry> *bar = barOf(ctx, thisVal);
        if (bar == nullptr)
            return ScriptApi::fromContext(ctx)->throwError("EngineError", "");
        for (ScriptApi::LocatorEntry &entry: *bar) {
            entry.mState = std::make_shared<ScriptWaypointState>(*entry.mState);
            entry.mState->mRemoved = true;
        }
        return JS_UNDEFINED;
    }

    JSValue locatorHasWaypoint(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const std::shared_ptr<ScriptWaypointState> state = argc >= 1 ? stateOf(ctx, argv[0]) : nullptr;
        if (state == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        std::vector<ScriptApi::LocatorEntry> *bar = barOf(ctx, thisVal);
        return JS_NewBool(ctx, bar != nullptr && findEntry(*bar, state) != bar->end());
    }

    JSValue locatorGetAllWaypoints(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::vector<ScriptApi::LocatorEntry> *bar = barOf(ctx, thisVal);
        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (size_t entry = 0; bar != nullptr && entry < bar->size(); ++entry) {
            if (!(*bar)[entry].mState->mRemoved)
                JS_SetPropertyUint32(ctx, list, index++, JS_DupValue(ctx, (*bar)[entry].mObject));
        }
        return list;
    }

    JSValue locatorGetCount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::vector<ScriptApi::LocatorEntry> *bar = barOf(ctx, thisVal);
        return JS_NewInt32(ctx, bar == nullptr ? 0 : (int32_t) liveCount(*bar));
    }

    JSValue locatorGetMaxCount(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewInt32(ctx, MAX_WAYPOINTS);
    }

    // the first bounds holding the distance: above the lower bound, up to the upper one
    const ScriptWaypointTexture *textureAt(const ScriptWaypointState &state, double distance) {
        for (const ScriptWaypointTexture &texture: state.mTextures) {
            if (distance > texture.mLowerBound && (!texture.mHasUpperBound || distance <= texture.mUpperBound))
                return &texture;
        }
        return nullptr;
    }

    bool visibleTo(const ScriptWaypointState &state, const Actor *actor, const ServerPlayer &viewer) {
        if (actor == nullptr)
            return state.mKind == ScriptWaypointState::Kind::Location;
        if (!state.mShowDead && !actor->isAlive())
            return false;
        if (!state.mShowInvisible && actor->hasEffect(MobEffectId::Invisibility))
            return false;
        if (!state.mShowSneaking && actor->getFlags().get(ActorFlag::Sneaking))
            return false;
        const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(actor);
        if (player != nullptr && player->isSpectator()) {
            if (!state.mShowSpectator)
                return false;
            if (!state.mShowSpectatorToSpectator && viewer.isSpectator())
                return false;
        }
        return true;
    }

    // RGB channels as an opaque ARGB colour
    int32_t packColor(const ScriptWaypointState &state) {
        const auto channel = [](double value) {
            return (uint32_t) std::lround(std::min(1.0, std::max(0.0, value)) * 255.0);
        };
        return (int32_t) (0xFF000000u | (channel(state.mRed) << 16) | (channel(state.mGreen) << 8) | channel(state.mBlue));
    }
}

// random high bits: every pack hands out handles to the same client
Uuid ScriptApi::nextWaypointHandle() {
    static std::mt19937_64 random(std::random_device{}());
    return Uuid(random(), ++mNextWaypointHandle);
}

void ScriptApi::_bindWaypoints() {
    JSValue waypoint = defineClass("Waypoint");
    property(waypoint, "Waypoint", "isValid", waypointGetIsValid);
    property(waypoint, "Waypoint", "isEnabled", waypointGetIsEnabled, waypointSetIsEnabled);
    property(waypoint, "Waypoint", "color", waypointGetColor, waypointSetColor);
    property(waypoint, "Waypoint", "textureSelector", waypointGetTextureSelector, waypointSetTextureSelector);
    method(waypoint, "Waypoint", "getDimensionLocation", waypointGetDimensionLocation, 0);
    method(waypoint, "Waypoint", "remove", waypointRemove, 0);

    JSValue location = defineClass("LocationWaypoint", locationWaypointConstructor, 3);
    method(location, "LocationWaypoint", "setDimensionLocation", locationWaypointSetDimensionLocation, 1);

    JSValue entity = defineClass("EntityWaypoint", entityWaypointConstructor, 4);
    property(entity, "EntityWaypoint", "entity", entityWaypointGetEntity);
    property(entity, "EntityWaypoint", "entityRules", entityWaypointGetEntityRules);

    JSValue player = defineClass("PlayerWaypoint", playerWaypointConstructor, 4);
    property(player, "PlayerWaypoint", "playerRules", playerWaypointGetPlayerRules);

    JSValue locator = defineClass("LocatorBar");
    property(locator, "LocatorBar", "count", locatorGetCount);
    property(locator, "LocatorBar", "maxCount", locatorGetMaxCount);
    method(locator, "LocatorBar", "addWaypoint", locatorAddWaypoint, 1);
    method(locator, "LocatorBar", "removeWaypoint", locatorRemoveWaypoint, 1);
    method(locator, "LocatorBar", "removeAllWaypoints", locatorRemoveAllWaypoints, 0);
    method(locator, "LocatorBar", "hasWaypoint", locatorHasWaypoint, 1);
    method(locator, "LocatorBar", "getAllWaypoints", locatorGetAllWaypoints, 0);
}

// only what changed since the last packet goes out: Add once, then Update, Remove at the end
void ScriptApi::_tickLocatorBars() {
    for (auto bar = mLocatorBars.begin(); bar != mLocatorBars.end(); ++bar) {
        ServerPlayer *viewer = resolvePlayerByHandle(bar->first);
        if (viewer == nullptr || bar->second.empty())
            continue;

        LocatorBarPacket packet;
        std::vector<LocatorEntry> &entries = bar->second;
        for (size_t index = 0; index < entries.size();) {
            LocatorEntry &entry = entries[index];
            const ScriptWaypointState &state = *entry.mState;
            if (state.mRemoved) {
                if (entry.mSent) {
                    LocatorBarPayload payload;
                    payload.mActionFlag = LocatorBarAction::Remove;
                    payload.mGroupHandle = entry.mHandle;
                    packet.mWaypoints.push_back(payload);
                }
                JS_FreeValue(mContext, entry.mObject);
                entries.erase(entries.begin() + (std::ptrdiff_t) index);
                continue;
            }

            Actor *actor = trackedActor(*this, state);
            LocatorBarWorldPosition position;
            position.mPosition = actor != nullptr ? actor->getPosition()
                                                  : Vector3f((float) state.mX, (float) state.mY, (float) state.mZ);
            position.mDimension = Dimension::toId(actor != nullptr ? actor->getDimension() : state.mDimension);
            if (actor == nullptr && entry.mSentWaypoint.mWorldPosition.has_value() && state.mKind != ScriptWaypointState::Kind::Location)
                position = entry.mSentWaypoint.mWorldPosition.value();

            double distance = std::numeric_limits<double>::infinity();
            if (Dimension::toId(viewer->getDimension()) == position.mDimension) {
                const Vector3f &from = viewer->getPosition();
                const double dx = from.x - position.mPosition.x, dy = from.y - position.mPosition.y,
                             dz = from.z - position.mPosition.z;
                distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
            const ScriptWaypointTexture *texture = textureAt(state, distance);

            LocatorBarWaypoint current;
            current.mVisible = state.mEnabled && texture != nullptr && visibleTo(state, actor, *viewer);
            current.mWorldPosition = position;
            if (texture != nullptr) {
                const BuiltInTexture *builtIn = texture->mCustom ? nullptr : findBuiltIn(texture->mTexture);
                current.mTexturePath = builtIn != nullptr ? std::string(builtIn->mPath) : texture->mTexture;
                current.mIconSize = builtIn != nullptr ? Vector2f(builtIn->mScale, builtIn->mScale)
                                                       : Vector2f((float) texture->mIconWidth, (float) texture->mIconHeight);
            } else if (entry.mSentWaypoint.mTexturePath.has_value()) {
                current.mTexturePath = entry.mSentWaypoint.mTexturePath;
                current.mIconSize = entry.mSentWaypoint.mIconSize;
            }
            if (state.mHasColor)
                current.mColor = packColor(state);

            int32_t flags = 0;
            const LocatorBarWaypoint &sent = entry.mSentWaypoint;
            if (!entry.mSent || !(sent.mWorldPosition->mPosition == position.mPosition)
                || sent.mWorldPosition->mDimension != position.mDimension)
                flags |= UPDATE_POSITION;
            if (!entry.mSent || sent.mVisible != current.mVisible)
                flags |= UPDATE_VISIBLE;
            if (current.mTexturePath.has_value() && (!entry.mSent || sent.mTexturePath != current.mTexturePath))
                flags |= UPDATE_TEXTURE;
            if (current.mIconSize.has_value()
                && (!entry.mSent || !sent.mIconSize.has_value() || !(sent.mIconSize.value() == current.mIconSize.value())))
                flags |= UPDATE_ICON_SIZE;
            if (current.mColor.has_value() && (!entry.mSent || sent.mColor != current.mColor))
                flags |= UPDATE_COLOR;
            ++index;
            if (flags == 0)
                continue;

            LocatorBarPayload payload;
            payload.mActionFlag = entry.mSent ? LocatorBarAction::Update : LocatorBarAction::Add;
            payload.mGroupHandle = entry.mHandle;
            payload.mWaypoint.mUpdateFlag = flags;
            if (flags & UPDATE_POSITION)
                payload.mWaypoint.mWorldPosition = current.mWorldPosition;
            if (flags & UPDATE_VISIBLE)
                payload.mWaypoint.mVisible = current.mVisible;
            if (flags & UPDATE_TEXTURE)
                payload.mWaypoint.mTexturePath = current.mTexturePath;
            if (flags & UPDATE_ICON_SIZE)
                payload.mWaypoint.mIconSize = current.mIconSize;
            if (flags & UPDATE_COLOR)
                payload.mWaypoint.mColor = current.mColor;
            packet.mWaypoints.push_back(payload);
            entry.mSentWaypoint = current;
            entry.mSent = true;
        }

        if (!packet.mWaypoints.empty())
            mHost.sendPacketTo(viewer->getNetworkIdentifier(), packet);
    }
}
