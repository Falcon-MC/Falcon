#include "Scripting/Binding/ScriptApi.h"

#include "Network/ServerNetworkHandler.h"
#include "Scripting/ScriptEngine.h"

#include <climits>
#include <cmath>

using namespace ScriptValues;

namespace {
    WorldClockSystem &clocksOf(ScriptApi &api) {
        return api.host().getScriptEngine().worldClocks();
    }

    const ScriptClockRef *refOf(JSContext *ctx, JSValueConst thisVal) {
        return ScriptApi::fromContext(ctx)->native<ScriptClockRef>(thisVal);
    }

    bool readTicks(JSContext *ctx, JSValueConst value, int32_t &ticks) {
        double number = 0.0;
        if (!JS_IsNumber(value) || JS_ToFloat64(ctx, &number, value) != 0 || !std::isfinite(number))
            return false;
        ticks = (int32_t) std::max(0.0, std::min((double) INT32_MAX, std::floor(number)));
        return number >= 0.0;
    }

    // TimeMarkerOptions: name, time, optional period
    bool readMarker(JSContext *ctx, JSValueConst options, WorldClockMarker &marker) {
        if (!JS_IsObject(options))
            return false;
        JSValue name = JS_GetPropertyStr(ctx, options, "name");
        JSValue time = JS_GetPropertyStr(ctx, options, "time");
        JSValue period = JS_GetPropertyStr(ctx, options, "period");
        bool ok = JS_IsString(name) && readTicks(ctx, time, marker.mTime);
        marker.mName = ok ? toString(ctx, name) : std::string();
        marker.mHasPeriod = ok && !JS_IsUndefined(period);
        if (marker.mHasPeriod)
            ok = readTicks(ctx, period, marker.mPeriod);
        JS_FreeValue(ctx, name);
        JS_FreeValue(ctx, time);
        JS_FreeValue(ctx, period);
        return ok;
    }

    // a TimeMarker or its name
    std::string markerName(JSContext *ctx, JSValueConst value) {
        const ScriptClockRef *ref = ScriptApi::fromContext(ctx)->native<ScriptClockRef>(value);
        if (ref != nullptr && !ref->mMarker.empty())
            return ref->mMarker;
        return JS_IsString(value) ? toString(ctx, value) : std::string();
    }

    JSValue registryRegisterClock(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsString(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::string name = toString(ctx, argv[0]);
        // sub_14C8A8710
        if (!api->registrationOpen())
            return api->throwError("WorldClockInvalidRegistryError",
                                   "Cannot register world clock '" + name + "' outside of StartupEvent.");

        std::vector<WorldClockMarker> markers;
        if (argc >= 2 && JS_IsObject(argv[1])) {
            JSValue list = JS_GetPropertyStr(ctx, argv[1], "timeMarkers");
            uint32_t length = 0;
            if (JS_IsArray(list)) {
                JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
                JS_ToUint32(ctx, &length, lengthValue);
                JS_FreeValue(ctx, lengthValue);
            }
            for (uint32_t index = 0; index < length; ++index) {
                JSValue item = JS_GetPropertyUint32(ctx, list, index);
                WorldClockMarker marker;
                const bool ok = readMarker(ctx, item, marker);
                JS_FreeValue(ctx, item);
                if (!ok) {
                    JS_FreeValue(ctx, list);
                    return JS_ThrowTypeError(ctx, "Native type conversion failed.");
                }
                markers.push_back(marker);
            }
            JS_FreeValue(ctx, list);
        }

        bool markerError = false;
        const WorldClockError error = clocksOf(*api).registerClock(name, markers, markerError);
        if (error != WorldClockError::None)
            return api->throwError("WorldClockRegistrationError", "Failed to register world clock '" + name + "'. "
                                                                          + WorldClockSystem::reasonText(error));
        return JS_UNDEFINED;
    }

    JSValue worldGetClock(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsString(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::string name = toString(ctx, argv[0]);
        if (clocksOf(*api).find(name) == nullptr)
            return api->throwError("WorldClockNotFoundError", "Failed to find world clock '" + name + "'.");
        return api->makeClock(name);
    }

    WorldClock *clockOf(JSContext *ctx, JSValueConst thisVal) {
        const ScriptClockRef *ref = refOf(ctx, thisVal);
        return ref == nullptr ? nullptr : clocksOf(*ScriptApi::fromContext(ctx)).find(ref->mClock);
    }

    JSValue clockGetName(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptClockRef *ref = refOf(ctx, thisVal);
        return ref == nullptr ? JS_UNDEFINED : string(ctx, ref->mClock);
    }

    JSValue clockGetTime(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const WorldClock *clock = clockOf(ctx, thisVal);
        return clock == nullptr ? JS_UNDEFINED : JS_NewInt32(ctx, clock->mTime);
    }

    JSValue clockSetTime(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        WorldClock *clock = clockOf(ctx, thisVal);
        int32_t ticks = 0;
        if (clock == nullptr || argc < 1 || !readTicks(ctx, argv[0], ticks))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        clocksOf(*api).setTime(*clock, ticks);
        return JS_UNDEFINED;
    }

    JSValue clockGetPaused(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const WorldClock *clock = clockOf(ctx, thisVal);
        return clock == nullptr ? JS_UNDEFINED : JS_NewBool(ctx, clock->mPaused);
    }

    JSValue clockSetPaused(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        WorldClock *clock = clockOf(ctx, thisVal);
        if (clock == nullptr || argc < 1 || !JS_IsBool(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        clocksOf(*ScriptApi::fromContext(ctx)).setPaused(*clock, JS_ToBool(ctx, argv[0]) == 1);
        return JS_UNDEFINED;
    }

    JSValue clockGetMarkers(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const WorldClock *clock = clockOf(ctx, thisVal);
        JSValue list = JS_NewArray(ctx);
        if (clock == nullptr)
            return list;
        uint32_t index = 0;
        for (const WorldClockMarker &marker: clock->mMarkers)
            JS_SetPropertyUint32(ctx, list, index++, api->makeTimeMarker(clock->mName, marker.mName));
        return list;
    }

    JSValue clockAddMarker(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        WorldClock *clock = clockOf(ctx, thisVal);
        WorldClockMarker marker;
        if (clock == nullptr || argc < 1 || !readMarker(ctx, argv[0], marker))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const WorldClockError error = clocksOf(*api).addMarker(*clock, marker);
        if (error != WorldClockError::None)
            return api->throwError("WorldClockAddTimeMarkerError", "Failed to add time marker '" + marker.mName
                                                                           + "' on world clock '" + clock->mName + "' "
                                                                           + WorldClockSystem::reasonText(error));
        return JS_UNDEFINED;
    }

    // the marker an argument names on this clock; throws WorldClockTimeMarkerNotFoundError otherwise
    const WorldClockMarker *markerArgument(JSContext *ctx, WorldClock &clock, int argc, JSValueConst *argv,
                                           JSValue &thrown) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::string name = argc >= 1 ? markerName(ctx, argv[0]) : std::string();
        const WorldClockMarker *marker = clocksOf(*api).findMarker(clock, name);
        if (marker == nullptr)
            thrown = api->throwError("WorldClockTimeMarkerNotFoundError",
                                     "Failed to find time marker '" + name + "' in world clock '" + clock.mName + "'.");
        return marker;
    }

    JSValue clockRemoveMarker(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        WorldClock *clock = clockOf(ctx, thisVal);
        if (clock == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        JSValue thrown = JS_UNDEFINED;
        const WorldClockMarker *marker = markerArgument(ctx, *clock, argc, argv, thrown);
        if (marker == nullptr)
            return thrown;
        // sub_14C9797B0
        if (marker->mName.compare(0, 10, "minecraft:") == 0)
            return api->throwError("WorldClockRemoveMinecraftTimeMarkerError",
                                   "Failed to remove time marker '" + marker->mName + "' from world clock '"
                                           + clock->mName
                                           + "'. A time marker with the 'minecraft' namespace cannot be removed.");
        clocksOf(*api).removeMarker(*clock, marker->mName);
        return JS_UNDEFINED;
    }

    // magic: 0 set, 1 rewindTo, 2 skipTo
    JSValue clockMove(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        WorldClock *clock = clockOf(ctx, thisVal);
        if (clock == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        JSValue thrown = JS_UNDEFINED;
        const WorldClockMarker *found = markerArgument(ctx, *clock, argc, argv, thrown);
        if (found == nullptr)
            return thrown;

        const WorldClockMarker marker = *found;
        WorldClockSystem &clocks = clocksOf(*api);
        if (magic == 0)
            clocks.setTime(*clock, marker.mTime);
        else if (magic == 2)
            clocks.skipTo(*clock, marker);
        else if (!clocks.rewindTo(*clock, marker))
            return api->throwError("WorldClockRewindError", "Failed to rewind world clock '" + clock->mName
                                                                    + "'. Current time is behind time marker '"
                                                                    + marker.mName + "'");
        return JS_UNDEFINED;
    }

    // magic: 0 name, 1 time, 2 period
    JSValue markerGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptClockRef *ref = refOf(ctx, thisVal);
        if (ref == nullptr)
            return JS_UNDEFINED;
        if (magic == 0)
            return string(ctx, ref->mMarker);

        WorldClock *clock = clocksOf(*api).find(ref->mClock);
        const WorldClockMarker *marker = clock == nullptr ? nullptr : clocksOf(*api).findMarker(*clock, ref->mMarker);
        if (marker == nullptr)
            return api->throwError("WorldClockInvalidTimeMarkerError", "");
        if (magic == 1)
            return JS_NewInt32(ctx, marker->mTime);
        return marker->mHasPeriod ? JS_NewInt32(ctx, marker->mPeriod) : JS_UNDEFINED;
    }
}

JSValue ScriptApi::makeClock(const std::string &name) {
    const auto existing = mClockObjects.find(name);
    if (existing != mClockObjects.end())
        return JS_DupValue(mContext, existing->second);

    ScriptClockRef *ref = new ScriptClockRef();
    ref->mClock = name;
    JSValue object = makeInstance("WorldClock", ref);
    mClockObjects[name] = JS_DupValue(mContext, object);
    return object;
}

JSValue ScriptApi::makeTimeMarker(const std::string &clock, const std::string &marker) {
    ScriptClockRef *ref = new ScriptClockRef();
    ref->mClock = clock;
    ref->mMarker = marker;
    return makeInstance("TimeMarker", ref);
}

void ScriptApi::_bindClocks() {
    if (mSurface.has("WorldClockRegistry")) {
        JSValue registry = defineClass("WorldClockRegistry");
        method(registry, "WorldClockRegistry", "registerClock", registryRegisterClock, 2);
        mClockRegistry = makeInstance("WorldClockRegistry", new ScriptRegistryRef());
    }

    if (mSurface.has("WorldClock")) {
        JSValue clock = defineClass("WorldClock");
        property(clock, "WorldClock", "name", clockGetName);
        property(clock, "WorldClock", "time", clockGetTime, clockSetTime);
        property(clock, "WorldClock", "isPaused", clockGetPaused, clockSetPaused);
        property(clock, "WorldClock", "timeMarkers", clockGetMarkers);
        method(clock, "WorldClock", "addTimeMarker", clockAddMarker, 1);
        method(clock, "WorldClock", "removeTimeMarker", clockRemoveMarker, 1);
        methodMagic(clock, "WorldClock", "set", clockMove, 1, 0);
        methodMagic(clock, "WorldClock", "rewindTo", clockMove, 1, 1);
        methodMagic(clock, "WorldClock", "skipTo", clockMove, 1, 2);
    }

    if (mSurface.has("TimeMarker")) {
        JSValue marker = defineClass("TimeMarker");
        propertyMagic(marker, "TimeMarker", "name", markerGet, 0);
        propertyMagic(marker, "TimeMarker", "time", markerGet, 1);
        propertyMagic(marker, "TimeMarker", "period", markerGet, 2);
    }

    JSValue world = prototypeOf("World");
    if (!JS_IsUndefined(world))
        method(world, "World", "getClock", worldGetClock, 1);
}
