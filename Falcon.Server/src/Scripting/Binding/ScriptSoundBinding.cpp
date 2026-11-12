#include "Scripting/Binding/ScriptApi.h"

#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ClientboundUpdateSoundDataPacket.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptSoundRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

#include <cmath>

using namespace ScriptValues;

namespace {
    const double MIN_PITCH = 0.009999999776482582;
    const double MAX_PITCH = 10.0;
    const double MAX_VOLUME = 10.0;
    const double MAX_SEEK = 107374184.0;

    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    // argument: the index of the refused argument, -1 for a property of the options
    JSValue outOfBounds(JSContext *ctx, const char *errorClass, double value, double minValue, double maxValue,
                        bool hasMax, int argument = -1) {
        JSValue fields = JS_NewObject(ctx);
        if (argument >= 0)
            JS_SetPropertyStr(ctx, fields, "index", JS_NewInt32(ctx, argument));
        JS_SetPropertyStr(ctx, fields, "minValue", JS_NewFloat64(ctx, minValue));
        if (hasMax)
            JS_SetPropertyStr(ctx, fields, "maxValue", JS_NewFloat64(ctx, maxValue));
        JS_SetPropertyStr(ctx, fields, "value", JS_NewFloat64(ctx, value));
        return ScriptApi::fromContext(ctx)->throwError(errorClass, "", fields);
    }

    ScriptSoundState *soundOf(JSContext *ctx, JSValueConst thisVal) {
        const ScriptSoundRef *ref = ScriptApi::fromContext(ctx)->native<ScriptSoundRef>(thisVal);
        return ref == nullptr ? nullptr : ref->mState.get();
    }

    // the loops already played do not count: the position is within the current play
    double playbackUnits(const ScriptSoundState &state) {
        if (!state.mHasDuration || state.mDurationUnits <= 0)
            return state.mElapsedUnits;
        return std::fmod(state.mElapsedUnits, (double) state.mDurationUnits);
    }

    // only sounds with a duration and a finite loop count are tracked (sub_141ECB3C0, sub_14C555130, 26.60)
    bool isActive(const ScriptSoundState &state) {
        return state.mHasDuration && state.mLoopCount >= 0 && !state.mStopped && !state.mCompleted;
    }

    // whole ticks or milliseconds; untracked, it reports the full duration (sub_14380C5C0, 26.60)
    float playbackPosition(const ScriptSoundState &state) {
        if (!isActive(state))
            return state.durationSeconds();
        return (float) std::floor(playbackUnits(state)) / (state.mRealTime ? 1000.0f : 20.0f);
    }

    bool readNumberArgument(JSContext *ctx, int argc, JSValueConst *argv, int index, double &value) {
        return argc > index && JS_IsNumber(argv[index]) && JS_ToFloat64(ctx, &value, argv[index]) == 0;
    }

    void sendUpdate(JSContext *ctx, const ScriptSoundState &state, ClientboundUpdateSoundDataPacket &packet) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        packet.mServerSoundHandle = state.mHandle;
        for (const uint32_t listener: state.mListeners) {
            ServerPlayer *player = api->resolvePlayerByHandle(listener);
            if (player != nullptr)
                api->host().sendPacketTo(player->getNetworkIdentifier(), packet);
        }
    }

    JSValue soundGetId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundState *state = soundOf(ctx, thisVal);
        return state == nullptr ? typeError(ctx) : string(ctx, std::to_string(state->mHandle));
    }

    JSValue soundGetEventId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundState *state = soundOf(ctx, thisVal);
        return state == nullptr ? typeError(ctx) : string(ctx, state->mEventId);
    }

    JSValue soundGetRecipient(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundState *state = soundOf(ctx, thisVal);
        if (state == nullptr)
            return typeError(ctx);
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = state->mRecipient == 0 ? nullptr : api->resolvePlayerByHandle(state->mRecipient);
        return player == nullptr ? JS_UNDEFINED : api->makePlayer(*player);
    }

    JSValue soundGetDurationInfo(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptSoundRef *ref = api->native<ScriptSoundRef>(thisVal);
        if (ref == nullptr)
            return typeError(ctx);
        if (!ref->mState->mHasDuration)
            return JS_UNDEFINED;
        ScriptSoundRef *info = new ScriptSoundRef();
        info->mState = ref->mState;
        return api->makeInstance("SoundDurationInfo", info);
    }

    JSValue soundStop(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptSoundState *state = soundOf(ctx, thisVal);
        if (state == nullptr)
            return typeError(ctx);
        ClientboundUpdateSoundDataPacket packet;
        packet.mType = SoundDataUpdateType::Stop;
        sendUpdate(ctx, *state, packet);
        state->mStopped = true;
        return JS_UNDEFINED;
    }

    JSValue soundPause(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptSoundState *state = soundOf(ctx, thisVal);
        if (state == nullptr)
            return typeError(ctx);
        ClientboundUpdateSoundDataPacket packet;
        packet.mType = SoundDataUpdateType::Pause;
        sendUpdate(ctx, *state, packet);
        state->mPaused = true;
        return JS_UNDEFINED;
    }

    JSValue soundResume(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptSoundState *state = soundOf(ctx, thisVal);
        if (state == nullptr)
            return typeError(ctx);
        ClientboundUpdateSoundDataPacket packet;
        packet.mType = SoundDataUpdateType::Resume;
        sendUpdate(ctx, *state, packet);
        state->mPaused = false;
        return JS_UNDEFINED;
    }

    JSValue soundSeekTo(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptSoundState *state = soundOf(ctx, thisVal);
        double seconds = 0.0;
        if (state == nullptr || !readNumberArgument(ctx, argc, argv, 0, seconds))
            return typeError(ctx);
        // the known duration bounds the float seconds (sub_14C55BBC0, 26.60)
        const double maxValue = state->mHasDuration ? (double) state->durationSeconds() : MAX_SEEK;
        if (seconds < 0.0 || (state->mHasDuration ? (float) seconds > state->durationSeconds() : seconds > MAX_SEEK))
            return outOfBounds(ctx, "ArgumentOutOfBoundsError", seconds, 0.0, maxValue, true, 0);
        ClientboundUpdateSoundDataPacket packet;
        packet.mType = SoundDataUpdateType::SeekTo;
        packet.mSeekTo.mSeconds = (float) seconds;
        sendUpdate(ctx, *state, packet);
        state->mElapsedUnits = state->mElapsedUnits - playbackUnits(*state) + seconds * state->unitsPerSecond();
        return JS_UNDEFINED;
    }

    JSValue soundSetPitch(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptSoundState *state = soundOf(ctx, thisVal);
        double pitch = 0.0;
        if (state == nullptr || !readNumberArgument(ctx, argc, argv, 0, pitch))
            return typeError(ctx);
        if (pitch < MIN_PITCH || pitch > MAX_PITCH)
            return outOfBounds(ctx, "ArgumentOutOfBoundsError", pitch, MIN_PITCH, MAX_PITCH, true, 0);
        ClientboundUpdateSoundDataPacket packet;
        packet.mType = SoundDataUpdateType::SetPitch;
        packet.mPitch.mPitch = (float) pitch;
        sendUpdate(ctx, *state, packet);
        return JS_UNDEFINED;
    }

    JSValue soundSetVolume(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptSoundState *state = soundOf(ctx, thisVal);
        double volume = 0.0;
        if (state == nullptr || !readNumberArgument(ctx, argc, argv, 0, volume))
            return typeError(ctx);
        if (volume < 0.0 || volume > MAX_VOLUME)
            return outOfBounds(ctx, "ArgumentOutOfBoundsError", volume, 0.0, MAX_VOLUME, true, 0);
        ClientboundUpdateSoundDataPacket packet;
        packet.mType = SoundDataUpdateType::SetVolume;
        packet.mVolume.mVolume = (float) volume;
        sendUpdate(ctx, *state, packet);
        return JS_UNDEFINED;
    }

    JSValue soundFade(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptSoundState *state = soundOf(ctx, thisVal);
        double duration = 0.0;
        double target = 0.0;
        if (state == nullptr || !readNumberArgument(ctx, argc, argv, 0, duration)
            || !readNumberArgument(ctx, argc, argv, 1, target))
            return typeError(ctx);
        if (duration < 0.0)
            return outOfBounds(ctx, "ArgumentOutOfBoundsError", duration, 0.0, 0.0, false, 0);
        if (target < 0.0)
            return outOfBounds(ctx, "ArgumentOutOfBoundsError", target, 0.0, 0.0, false, 1);
        ClientboundUpdateSoundDataPacket packet;
        packet.mType = SoundDataUpdateType::Fade;
        packet.mFade.mDuration = (float) duration;
        packet.mFade.mTargetVolume = (float) target;
        sendUpdate(ctx, *state, packet);
        return JS_UNDEFINED;
    }

    JSValue durationGetDuration(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundState *state = soundOf(ctx, thisVal);
        // a float of ticks / 20 or milliseconds / 1000 (sub_14C554800, 26.60)
        return state == nullptr ? typeError(ctx) : JS_NewFloat64(ctx, state->durationSeconds());
    }

    JSValue durationGetIsActive(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundState *state = soundOf(ctx, thisVal);
        return state == nullptr ? typeError(ctx) : JS_NewBool(ctx, isActive(*state));
    }

    JSValue durationGetPlaybackPosition(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundState *state = soundOf(ctx, thisVal);
        return state == nullptr ? typeError(ctx) : JS_NewFloat64(ctx, playbackPosition(*state));
    }

    const ScriptSoundDefinition *definitionOf(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptSoundDefinitionRef *ref = api->native<ScriptSoundDefinitionRef>(thisVal);
        return ref == nullptr ? nullptr : api->host().getScriptEngine().sounds().find(ref->mEventId);
    }

    JSValue stringArray(JSContext *ctx, const std::vector<std::string> &values) {
        JSValue array = JS_NewArray(ctx);
        for (uint32_t index = 0; index < values.size(); ++index)
            JS_SetPropertyUint32(ctx, array, index, string(ctx, values[index]));
        return array;
    }

    JSValue definitionGetEventId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundDefinition *definition = definitionOf(ctx, thisVal);
        return definition == nullptr ? typeError(ctx) : string(ctx, definition->mEventId);
    }

    JSValue definitionGetDurationInfo(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundDefinition *definition = definitionOf(ctx, thisVal);
        if (definition == nullptr)
            return typeError(ctx);
        if (!definition->mHasDuration)
            return JS_UNDEFINED;
        JSValue info = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, info, "duration", JS_NewFloat64(ctx, (double) definition->durationSeconds()));
        return info;
    }

    JSValue definitionGetMusicInfo(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundDefinition *definition = definitionOf(ctx, thisVal);
        if (definition == nullptr)
            return typeError(ctx);
        if (!definition->mHasMusic)
            return JS_UNDEFINED;
        JSValue info = JS_NewObject(ctx);
        if (definition->mHasArtist)
            JS_SetPropertyStr(ctx, info, "artist", string(ctx, definition->mArtist));
        if (definition->mHasGenres)
            JS_SetPropertyStr(ctx, info, "genres", stringArray(ctx, definition->mGenres));
        if (definition->mHasMoods)
            JS_SetPropertyStr(ctx, info, "moods", stringArray(ctx, definition->mMoods));
        if (definition->mHasTitle)
            JS_SetPropertyStr(ctx, info, "title", string(ctx, definition->mTitle));
        return info;
    }

    JSValue definitionGetTags(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptSoundDefinition *definition = definitionOf(ctx, thisVal);
        if (definition == nullptr)
            return typeError(ctx);
        if (!definition->mHasTags)
            return JS_UNDEFINED;
        JSValue tags = JS_NewObject(ctx);
        for (const auto &tag: definition->mTags)
            JS_SetPropertyStr(ctx, tags, tag.first.c_str(), stringArray(ctx, tag.second));
        return tags;
    }

    bool readStrings(JSContext *ctx, JSValueConst object, const char *key, std::vector<std::string> &values) {
        JSValue value = JS_GetPropertyStr(ctx, object, key);
        bool ok = true;
        if (!JS_IsUndefined(value)) {
            ok = JS_IsArray(value);
            uint32_t length = 0;
            JSValue lengthValue = ok ? JS_GetPropertyStr(ctx, value, "length") : JS_UNDEFINED;
            if (ok)
                JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);
            for (uint32_t index = 0; ok && index < length; ++index) {
                JSValue element = JS_GetPropertyUint32(ctx, value, index);
                ok = JS_IsString(element);
                if (ok)
                    values.push_back(toString(ctx, element));
                JS_FreeValue(ctx, element);
            }
        }
        JS_FreeValue(ctx, value);
        return ok;
    }

    bool readBound(JSContext *ctx, JSValueConst object, const char *key, bool &has, double &bound) {
        JSValue value = JS_GetPropertyStr(ctx, object, key);
        bool ok = true;
        if (!JS_IsUndefined(value)) {
            ok = JS_IsNumber(value) && JS_ToFloat64(ctx, &bound, value) == 0;
            has = ok;
        }
        JS_FreeValue(ctx, value);
        return ok;
    }

    bool readTagFilter(JSContext *ctx, JSValueConst object, ScriptSoundFilter &filter) {
        JSValue tags = JS_GetPropertyStr(ctx, object, "tags");
        bool ok = JS_IsUndefined(tags) || JS_IsObject(tags);
        if (ok && JS_IsObject(tags)) {
            JSPropertyEnum *names = nullptr;
            uint32_t count = 0;
            ok = JS_GetOwnPropertyNames(ctx, &names, &count, tags, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0;
            for (uint32_t index = 0; ok && index < count; ++index) {
                const char *name = JS_AtomToCString(ctx, names[index].atom);
                std::vector<std::string> values;
                ok = name != nullptr && readStrings(ctx, tags, name, values);
                if (ok)
                    filter.mTags.emplace_back(name, std::move(values));
                JS_FreeCString(ctx, name);
            }
            if (names != nullptr)
                JS_FreePropertyEnum(ctx, names, count);
        }
        JS_FreeValue(ctx, tags);
        return ok;
    }

    JSValue registryGetDefinitions(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptSoundFilter filter;
        if (argc >= 1 && !JS_IsUndefined(argv[0])) {
            if (!JS_IsObject(argv[0]) || !readStrings(ctx, argv[0], "artists", filter.mArtists)
                || !readStrings(ctx, argv[0], "titles", filter.mTitles)
                || !readStrings(ctx, argv[0], "genres", filter.mGenres)
                || !readStrings(ctx, argv[0], "moods", filter.mMoods)
                || !readBound(ctx, argv[0], "minDuration", filter.mHasMinDuration, filter.mMinDuration)
                || !readBound(ctx, argv[0], "maxDuration", filter.mHasMaxDuration, filter.mMaxDuration)
                || !readTagFilter(ctx, argv[0], filter))
                return typeError(ctx);
        }
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (filter.mHasMinDuration && filter.mHasMaxDuration && filter.mMinDuration > filter.mMaxDuration)
            return api->throwError("InvalidArgumentError", "SoundDefinitionFilter.minDuration must be less than or "
                                                           "equal to SoundDefinitionFilter.maxDuration");
        JSValue result = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const ScriptSoundDefinition *definition: api->host().getScriptEngine().sounds().definitions(filter)) {
            ScriptSoundDefinitionRef *ref = new ScriptSoundDefinitionRef();
            ref->mEventId = definition->mEventId;
            JS_SetPropertyUint32(ctx, result, index++, api->makeInstance("SoundDefinition", ref));
        }
        return result;
    }

    JSValue worldGetSoundDefinitionRegistry(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_DupValue(ctx, ScriptApi::fromContext(ctx)->soundDefinitionRegistry());
    }
}

// one counter for every pack: the handles reach the same clients
int64_t ScriptApi::nextSoundHandle() {
    static int64_t next = 0;
    return ++next;
}

JSValue ScriptApi::readSoundOptions(JSValueConst options, ScriptSoundOptions &read, bool world) {
    if (JS_IsUndefined(options))
        return JS_UNDEFINED;
    if (!JS_IsObject(options))
        return typeError(mContext);
    JSValue volume = JS_GetPropertyStr(mContext, options, "volume");
    JSValue pitch = JS_GetPropertyStr(mContext, options, "pitch");
    JSValue loop = JS_GetPropertyStr(mContext, options, "loopCount");
    JSValue broadcast = world ? JS_GetPropertyStr(mContext, options, "isBroadcast") : JS_UNDEFINED;
    double number = 0.0;
    bool ok = true;
    if (!JS_IsUndefined(volume) && (ok = JS_ToFloat64(mContext, &number, volume) == 0))
        read.mVolume = number;
    if (ok && !JS_IsUndefined(pitch) && (ok = JS_ToFloat64(mContext, &number, pitch) == 0))
        read.mPitch = number;
    if (ok && !JS_IsUndefined(loop))
        ok = JS_ToInt32(mContext, &read.mLoopCount, loop) == 0;
    if (ok && !JS_IsUndefined(broadcast))
        read.mBroadcast = JS_ToBool(mContext, broadcast) == 1;
    JS_FreeValue(mContext, volume);
    JS_FreeValue(mContext, pitch);
    JS_FreeValue(mContext, loop);
    JS_FreeValue(mContext, broadcast);
    if (!ok)
        return typeError(mContext);
    // compared as the floats the game keeps (sub_14D4FB100, 26.60)
    if ((float) read.mVolume < 0.0f)
        return outOfBounds(mContext, "PropertyOutOfBoundsError", read.mVolume, 0.0, 0.0, false);
    if ((float) read.mPitch < 0.01f)
        return outOfBounds(mContext, "PropertyOutOfBoundsError", read.mPitch, MIN_PITCH, 0.0, false);
    if (read.mLoopCount < -1)
        return outOfBounds(mContext, "PropertyOutOfBoundsError", read.mLoopCount, -1.0, 0.0, false);
    return JS_UNDEFINED;
}

JSValue ScriptApi::playSound(Level &level, ServerPlayer *recipient, const std::string &eventId,
                             const Vector3f &position, const ScriptSoundOptions &options) {
    std::shared_ptr<ScriptSoundState> state = std::make_shared<ScriptSoundState>();
    state->mHandle = nextSoundHandle();
    state->mEventId = eventId;
    state->mLoopCount = options.mLoopCount;
    const ScriptSoundDefinition *definition = mHost.getScriptEngine().sounds().find(eventId);
    if (definition != nullptr && definition->mHasDuration) {
        state->mHasDuration = true;
        state->mDurationUnits = definition->mDurationUnits;
        state->mRealTime = definition->mRealTime;
    }
    // isBroadcast: only players whose chunk view covers the sound, within a declared broadcast_range horizontally
    // Dimension::playSound (sub_14B53D880, 26.60)
    const bool ranged = options.mBroadcast && definition != nullptr && definition->mHasBroadcastRange;
    const float rangeSquared = ranged ? definition->mBroadcastRange * definition->mBroadcastRange : 0.0f;
    const Vector3i block((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                         (int32_t) std::floor(position.z));

    PlaySoundPacket packet;
    packet.mSound = eventId;
    packet.mPosition = position;
    packet.mVolume = (float) options.mVolume;
    packet.mPitch = (float) options.mPitch;
    packet.mLoopCount = options.mLoopCount;
    packet.mBypassListenerRangeCheck = options.mBroadcast;
    packet.mHasServerSoundHandle = true;
    packet.mServerSoundHandle = state->mHandle;

    if (recipient != nullptr) {
        state->mRecipient = mPlayers.acquire(recipient->getNetworkIdentifier());
        state->mListeners.push_back(state->mRecipient);
        mHost.sendPacketTo(recipient->getNetworkIdentifier(), packet);
    } else {
        for (auto &entry: mHost.getPlayers()) {
            if (!entry.second.isSpawned() || !entry.second.isIn(level))
                continue;
            if (ranged) {
                const Vector3f &at = entry.second.getPosition();
                const float dx = at.x - position.x, dz = at.z - position.z;
                if (dx * dx + dz * dz > rangeSquared)
                    continue;
            }
            if (options.mBroadcast && !ChunkStreamHandler::isPositionRelevant(entry.second, level, block))
                continue;
            state->mListeners.push_back(mPlayers.acquire(entry.first));
            mHost.sendPacketTo(entry.first, packet);
        }
    }

    if ((state->mHasDuration && state->mLoopCount >= 0) || state->mLoopCount < 0) {
        state->mLastRealTime = std::chrono::steady_clock::now();
        mTrackedSounds.push_back(state);
    }

    ScriptSoundRef *ref = new ScriptSoundRef();
    ref->mState = std::move(state);
    return makeInstance("SoundInstance", ref);
}

// finite sounds with a duration complete once every loop played; endless ones stop once scripts drop them
void ScriptApi::_tickSounds() {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    for (size_t index = 0; index < mTrackedSounds.size();) {
        std::shared_ptr<ScriptSoundState> &state = mTrackedSounds[index];
        if (state->mStopped) {
            mTrackedSounds.erase(mTrackedSounds.begin() + (std::ptrdiff_t) index);
            continue;
        }
        if (state->mLoopCount < 0 && state.use_count() == 1) {
            ClientboundUpdateSoundDataPacket packet;
            packet.mType = SoundDataUpdateType::Stop;
            sendUpdate(mContext, *state, packet);
            mTrackedSounds.erase(mTrackedSounds.begin() + (std::ptrdiff_t) index);
            continue;
        }
        const double delta = state->mRealTime
                                     ? std::chrono::duration<double, std::milli>(now - state->mLastRealTime).count()
                                     : 1.0;
        state->mLastRealTime = now;
        if (!state->mPaused)
            state->mElapsedUnits += delta;
        if (state->mHasDuration && state->mLoopCount >= 0
            && state->mElapsedUnits >= (double) state->mDurationUnits * (double) (state->mLoopCount + 1)) {
            state->mCompleted = true;
            const std::string id = std::to_string(state->mHandle);
            mTrackedSounds.erase(mTrackedSounds.begin() + (std::ptrdiff_t) index);
            mHost.getScriptEngine().onSoundCompleted(id);
            continue;
        }
        ++index;
    }
}

void ScriptApi::_bindSounds() {
    JSValue instance = defineClass("SoundInstance");
    property(instance, "SoundInstance", "id", soundGetId);
    property(instance, "SoundInstance", "soundEventId", soundGetEventId);
    property(instance, "SoundInstance", "recipient", soundGetRecipient);
    property(instance, "SoundInstance", "durationInfo", soundGetDurationInfo);
    method(instance, "SoundInstance", "stop", soundStop, 0);
    method(instance, "SoundInstance", "pause", soundPause, 0);
    method(instance, "SoundInstance", "resume", soundResume, 0);
    method(instance, "SoundInstance", "seekTo", soundSeekTo, 1);
    method(instance, "SoundInstance", "setPitch", soundSetPitch, 1);
    method(instance, "SoundInstance", "setVolume", soundSetVolume, 1);
    method(instance, "SoundInstance", "fade", soundFade, 2);

    JSValue duration = defineClass("SoundDurationInfo");
    property(duration, "SoundDurationInfo", "duration", durationGetDuration);
    property(duration, "SoundDurationInfo", "isActive", durationGetIsActive);
    method(duration, "SoundDurationInfo", "getPlaybackPosition", durationGetPlaybackPosition, 0);

    JSValue definition = defineClass("SoundDefinition");
    property(definition, "SoundDefinition", "soundEventId", definitionGetEventId);
    property(definition, "SoundDefinition", "durationInfo", definitionGetDurationInfo);
    property(definition, "SoundDefinition", "musicInfo", definitionGetMusicInfo);
    property(definition, "SoundDefinition", "tags", definitionGetTags);

    JSValue registry = defineClass("SoundDefinitionRegistry");
    method(registry, "SoundDefinitionRegistry", "getDefinitions", registryGetDefinitions, 1);
    mSoundDefinitionRegistry = makeInstance("SoundDefinitionRegistry", new ScriptRegistryRef());
    property(prototypeOf("World"), "World", "soundDefinitionRegistry", worldGetSoundDefinitionRegistry);
}
