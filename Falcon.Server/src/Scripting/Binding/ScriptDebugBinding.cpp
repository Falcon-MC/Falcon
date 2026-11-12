#include "Scripting/Binding/ScriptApi.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PrimitiveShapesPacket.h"
#include "Scripting/Binding/ScriptDebugBootstrap.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/Actor.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace ScriptValues;

namespace {
    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    void addFunction(JSContext *ctx, JSValue object, const char *name, JSCFunction *function, int length) {
        JS_SetPropertyStr(ctx, object, name, JS_NewCFunction(ctx, function, name, length));
    }

    double numberOf(JSContext *ctx, JSValueConst object, const char *name, double fallback = 0.0) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        double number = fallback;
        if (JS_IsNumber(value))
            JS_ToFloat64(ctx, &number, value);
        JS_FreeValue(ctx, value);
        return number;
    }

    bool hasProperty(JSContext *ctx, JSValueConst object, const char *name) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        const bool present = !JS_IsUndefined(value);
        JS_FreeValue(ctx, value);
        return present;
    }

    bool booleanOf(JSContext *ctx, JSValueConst object, const char *name) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        const bool result = JS_ToBool(ctx, value) == 1;
        JS_FreeValue(ctx, value);
        return result;
    }

    Vector3f vectorOf(JSContext *ctx, JSValueConst object, const char *name) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        Vector3f vector((float) numberOf(ctx, value, "0"), (float) numberOf(ctx, value, "1"),
                        (float) numberOf(ctx, value, "2"));
        JS_FreeValue(ctx, value);
        return vector;
    }

    Vector2f pairOf(JSContext *ctx, JSValueConst object, const char *name) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        Vector2f pair((float) numberOf(ctx, value, "0"), (float) numberOf(ctx, value, "1"));
        JS_FreeValue(ctx, value);
        return pair;
    }

    // mce::Color goes out as ARGB, one byte per channel
    uint32_t argbOf(JSContext *ctx, JSValueConst object, const char *name) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        uint32_t argb = 0;
        const char *channels[4] = {"3", "0", "1", "2"};
        for (const char *channel: channels) {
            const double component = std::min(1.0, std::max(0.0, numberOf(ctx, value, channel)));
            argb = (argb << 8) | (uint32_t) (component * 255.0);
        }
        JS_FreeValue(ctx, value);
        return argb;
    }

    // the drawer's shape snapshot: { id } for a removal, or every field the shape has
    DebugShapeData readShape(JSContext *ctx, JSValueConst shape) {
        DebugShapeData data;
        data.mId = (uint64_t) numberOf(ctx, shape, "id");
        if (!hasProperty(ctx, shape, "type"))
            return data;

        data.mHasType = true;
        data.mType = (DebugShapeType) (int32_t) numberOf(ctx, shape, "type");
        data.mHasPosition = true;
        data.mPosition = vectorOf(ctx, shape, "location");
        data.mHasDimension = true;
        data.mDimension = (int32_t) numberOf(ctx, shape, "dimension");
        data.mHasScale = true;
        data.mScale = (float) numberOf(ctx, shape, "scale");
        data.mHasRotation = true;
        data.mRotation = vectorOf(ctx, shape, "rotation");
        data.mHasTotalTimeLeft = true;
        data.mTotalTimeLeft = (float) numberOf(ctx, shape, "timeLeft");
        data.mHasMaximumRenderDistance = true;
        data.mMaximumRenderDistance = (float) numberOf(ctx, shape, "maximumRenderDistance");
        data.mHasColor = true;
        data.mColor = argbOf(ctx, shape, "color");
        if (hasProperty(ctx, shape, "attachedTo")) {
            data.mHasAttachedToActorId = true;
            data.mAttachedToActorId = (uint64_t) (int64_t) numberOf(ctx, shape, "attachedTo");
        }

        switch (data.mType) {
            case DebugShapeType::Line:
                data.mLineEndPosition = vectorOf(ctx, shape, "endLocation");
                break;
            case DebugShapeType::Arrow:
                data.mHasArrowEndPosition = true;
                data.mArrowEndPosition = vectorOf(ctx, shape, "endLocation");
                data.mHasArrowHeadLength = true;
                data.mArrowHeadLength = (float) numberOf(ctx, shape, "headLength");
                data.mHasArrowHeadRadius = true;
                data.mArrowHeadRadius = (float) numberOf(ctx, shape, "headRadius");
                data.mHasArrowHeadSegments = true;
                data.mArrowHeadSegments = (int32_t) numberOf(ctx, shape, "headSegments");
                break;
            case DebugShapeType::Box:
                data.mBoxBounds = vectorOf(ctx, shape, "bound");
                break;
            case DebugShapeType::Sphere:
            case DebugShapeType::Circle:
                data.mSegments = (int32_t) numberOf(ctx, shape, "segments");
                break;
            case DebugShapeType::Text: {
                JSValue text = JS_GetPropertyStr(ctx, shape, "text");
                data.mText = toString(ctx, text);
                JS_FreeValue(ctx, text);
                data.mUseRotation = booleanOf(ctx, shape, "useRotation");
                if (hasProperty(ctx, shape, "backgroundColor")) {
                    data.mHasBackgroundColor = true;
                    data.mBackgroundColor = argbOf(ctx, shape, "backgroundColor");
                }
                data.mLineGapHeight = (float) numberOf(ctx, shape, "lineGapHeight");
                data.mDepthTest = booleanOf(ctx, shape, "depthTest");
                data.mShowBackface = booleanOf(ctx, shape, "backfaceVisible");
                data.mShowTextBackface = booleanOf(ctx, shape, "textBackfaceVisible");
                break;
            }
            case DebugShapeType::Cylinder:
                data.mRadii2f = pairOf(ctx, shape, "radii");
                data.mHeight = (float) numberOf(ctx, shape, "height");
                data.mSegments = (int32_t) numberOf(ctx, shape, "segments");
                break;
            case DebugShapeType::Pyramid:
                data.mWidth = (float) numberOf(ctx, shape, "width");
                data.mHasDepth = hasProperty(ctx, shape, "depth");
                data.mDepth = (float) numberOf(ctx, shape, "depth");
                data.mHeight = (float) numberOf(ctx, shape, "height");
                break;
            case DebugShapeType::Ellipsoid:
                data.mRadii3f = vectorOf(ctx, shape, "radii");
                data.mSegments = (int32_t) numberOf(ctx, shape, "segments");
                break;
            case DebugShapeType::Cone:
                data.mRadii2f = pairOf(ctx, shape, "radii");
                data.mHeight = (float) numberOf(ctx, shape, "height");
                data.mSegments = (int32_t) numberOf(ctx, shape, "segments");
                break;
        }
        return data;
    }

    JSValue debugPrivilege(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewInt32(ctx, (int32_t) ScriptApi::fromContext(ctx)->privilege());
    }

    JSValue debugPlayers(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        JSValue players = JS_NewArray(ctx);
        uint32_t index = 0;
        for (auto &entry: api->host().getPlayers()) {
            if (entry.second.isSpawned())
                JS_SetPropertyUint32(ctx, players, index++, api->makePlayer(entry.second));
        }
        return players;
    }

    // (player, shapes): one packet carrying every snapshot for that player
    JSValue debugSend(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 2 || !JS_IsArray(argv[1]))
            return typeError(ctx);
        ServerPlayer *player = api->resolvePlayer(argv[0]);
        if (player == nullptr)
            return JS_UNDEFINED;

        PrimitiveShapesPacket packet;
        const uint32_t count = (uint32_t) numberOf(ctx, argv[1], "length");
        for (uint32_t index = 0; index < count; ++index) {
            JSValue shape = JS_GetPropertyUint32(ctx, argv[1], index);
            packet.mShapes.push_back(readShape(ctx, shape));
            JS_FreeValue(ctx, shape);
        }
        ServerNetworkHandler &host = api->host();
        host.getNetworkHandler().send(player->getNetworkIdentifier(), packet, host.getCodecContext());
        return JS_UNDEFINED;
    }

    JSValue debugIsPlayer(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptEntityRef *ref = argc >= 1 && JS_IsObject(argv[0]) ? api->native<ScriptEntityRef>(argv[0])
                                                                       : nullptr;
        return JS_NewBool(ctx, ref != nullptr && ref->mKind == ScriptEntityRef::Kind::Player);
    }

    JSValue debugIsEntity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        return JS_NewBool(ctx, argc >= 1 && JS_IsObject(argv[0]) && api->native<ScriptEntityRef>(argv[0]) != nullptr);
    }

    // the ActorUniqueID the client attaches to, undefined once the entity is gone
    JSValue debugEntityId(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Actor *actor = argc >= 1 ? api->resolveEntity(argv[0]) : nullptr;
        return actor == nullptr ? JS_UNDEFINED : JS_NewFloat64(ctx, (double) actor->getUniqueId());
    }

    // -1 for anything but a Dimension
    JSValue debugDimensionId(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptDimensionRef *ref = argc >= 1 && JS_IsObject(argv[0]) ? api->native<ScriptDimensionRef>(argv[0])
                                                                          : nullptr;
        return JS_NewInt32(ctx, ref == nullptr ? -1 : (int32_t) ref->mDimension);
    }

    JSValue debugRawText(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1)
            return typeError(ctx);
        bool isJson = false;
        return string(ctx, messageToRawText(ctx, argv[0], isJson));
    }

    JSValue debugNextShapeId(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        // network ids are shared by every pack's shapes
        static std::atomic<uint64_t> next(1);
        return JS_NewFloat64(ctx, (double) next++);
    }

    void setCount(JSContext *ctx, JSValue object, const char *name, int64_t value) {
        JS_SetPropertyStr(ctx, object, name, JS_NewFloat64(ctx, (double) value));
    }

    // RuntimeStats: QuickJS's own memory usage, field for field
    JSValue debugRuntimeStats(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSMemoryUsage usage;
        JS_ComputeMemoryUsage(JS_GetRuntime(ctx), &usage);
        JSValue stats = JS_NewObject(ctx);
        setCount(ctx, stats, "arrayCount", usage.array_count);
        setCount(ctx, stats, "atomCount", usage.atom_count);
        setCount(ctx, stats, "atomSize", usage.atom_size);
        setCount(ctx, stats, "fastArrayCount", usage.fast_array_count);
        setCount(ctx, stats, "fastArrayElementCount", usage.fast_array_elements);
        setCount(ctx, stats, "functionCodeSize", usage.js_func_code_size);
        setCount(ctx, stats, "functionCount", usage.js_func_count);
        setCount(ctx, stats, "functionLineCount", usage.js_func_pc2line_count);
        setCount(ctx, stats, "functionSize", usage.js_func_size);
        setCount(ctx, stats, "memoryAllocatedCount", usage.malloc_count);
        setCount(ctx, stats, "memoryAllocatedLimit", usage.malloc_limit);
        setCount(ctx, stats, "memoryAllocatedSize", usage.malloc_size);
        setCount(ctx, stats, "memoryUsedCount", usage.memory_used_count);
        setCount(ctx, stats, "memoryUsedSize", usage.memory_used_size);
        setCount(ctx, stats, "objectCount", usage.obj_count);
        setCount(ctx, stats, "objectSize", usage.obj_size);
        setCount(ctx, stats, "propertyCount", usage.prop_count);
        setCount(ctx, stats, "propertySize", usage.prop_size);
        setCount(ctx, stats, "stringCount", usage.str_count);
        setCount(ctx, stats, "stringSize", usage.str_size);
        return stats;
    }

    // every pack with scripts, its live handles by class (ScriptPluginStats)
    JSValue debugPluginStats(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        JSValue plugins = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::unique_ptr<ScriptApi> &other: api->host().getScriptEngine().getApis()) {
            JSValue plugin = JS_NewObject(ctx);
            JSValue counts = JS_NewObject(ctx);
            for (const auto &count: other->handleCounts()) {
                if (count.second > 0)
                    setCount(ctx, counts, count.first.c_str(), count.second);
            }
            JS_SetPropertyStr(ctx, plugin, "handleCounts", counts);
            JS_SetPropertyStr(ctx, plugin, "name", string(ctx, other->packName()));
            JS_SetPropertyStr(ctx, plugin, "packId", string(ctx, other->packUuid()));
            JS_SetPropertyStr(ctx, plugin, "scriptModuleUUID", string(ctx, other->scriptModuleUuid()));
            JS_SetPropertyUint32(ctx, plugins, index++, plugin);
        }
        JSValue stats = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, stats, "plugins", plugins);
        return stats;
    }

    JSValue debugTimingWarnings(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1 || !JS_IsBool(argv[0]))
            return typeError(ctx);
        ScriptApi::fromContext(ctx)->setTimingWarningsDisabled(JS_ToBool(ctx, argv[0]) == 1);
        return JS_UNDEFINED;
    }

    JSValue worldGetPrimitiveShapesManager(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_DupValue(ctx, ScriptApi::fromContext(ctx)->primitiveShapesManager());
    }

    JSValue runBootstrap(JSContext *ctx, const char *source, JSValue argument) {
        JSValue factory = JS_Eval(ctx, source, std::strlen(source), "<debug>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(factory)) {
            JS_FreeValue(ctx, argument);
            return factory;
        }
        JSValue result = JS_Call(ctx, factory, JS_UNDEFINED, 1, &argument);
        JS_FreeValue(ctx, factory);
        JS_FreeValue(ctx, argument);
        return result;
    }
}

void ScriptApi::_installDebugUtilities() {
    const bool debug = mModules.find("@minecraft/debug-utilities") != nullptr;
    JSValue native = JS_NewObject(mContext);
    addFunction(mContext, native, "privilege", debugPrivilege, 0);
    addFunction(mContext, native, "players", debugPlayers, 0);
    addFunction(mContext, native, "send", debugSend, 2);
    addFunction(mContext, native, "isPlayer", debugIsPlayer, 1);
    addFunction(mContext, native, "isEntity", debugIsEntity, 1);
    addFunction(mContext, native, "entityId", debugEntityId, 1);
    addFunction(mContext, native, "dimensionId", debugDimensionId, 1);
    addFunction(mContext, native, "rawText", debugRawText, 1);
    addFunction(mContext, native, "nextShapeId", debugNextShapeId, 0);
    addFunction(mContext, native, "runtimeStats", debugRuntimeStats, 0);
    addFunction(mContext, native, "pluginStats", debugPluginStats, 0);
    addFunction(mContext, native, "timingWarnings", debugTimingWarnings, 1);
    JS_SetPropertyStr(mContext, native, "errors", JS_DupValue(mContext, mErrors));

    JSValue exports = runBootstrap(mContext, ScriptDebugBootstrap::source(), native);
    if (JS_IsException(exports)) {
        logPendingException("<debug>");
        return;
    }

    JSPropertyEnum *names = nullptr;
    uint32_t count = 0;
    if (JS_GetOwnPropertyNames(mContext, &names, &count, exports, JS_GPN_STRING_MASK) == 0) {
        for (uint32_t index = 0; index < count; ++index) {
            const char *name = JS_AtomToCString(mContext, names[index].atom);
            if (name != nullptr && debug && mDebugSurface.has(name))
                mDebugExports.emplace_back(name, JS_GetProperty(mContext, exports, names[index].atom));
            if (name != nullptr)
                JS_FreeCString(mContext, name);
        }
        JS_FreePropertyEnum(mContext, names, count);
    }

    for (const char *name: {"PrimitiveShape", "TextPrimitive", "PrimitiveShapesManager"}) {
        if (mSurface.has(name))
            mServerExports.emplace_back(name, JS_GetPropertyStr(mContext, exports, name));
    }
    if (mSurface.has("World.primitiveShapesManager")) {
        mPrimitiveShapes = JS_GetPropertyStr(mContext, exports, "primitiveShapesManager");
        property(prototypeOf("World"), "World", "primitiveShapesManager", worldGetPrimitiveShapesManager);
    }

    for (const char *name: {"DiagnosticsChartDisplayType", "DiagnosticsErrorReason", "DiagnosticsTableDisplayType"}) {
        if (!debug || !mDebugSurface.has(name))
            continue;
        JSValue object = JS_NewObject(mContext);
        for (const auto &value: mDebugSurface.enumValues(name)) {
            const bool numeric = !value.second.empty()
                                 && value.second.find_first_not_of("0123456789") == std::string::npos;
            JS_DefinePropertyValueStr(mContext, object, value.first.c_str(),
                                      numeric ? JS_NewInt32(mContext, std::atoi(value.second.c_str()))
                                              : string(mContext, value.second),
                                      JS_PROP_ENUMERABLE);
        }
        mDebugExports.emplace_back(name, object);
    }

    // the drawer streams its shapes once per tick, like ServerScriptPrimitiveShapesSystem
    JSValue tick = JS_GetPropertyStr(mContext, exports, "tick");
    JS_FreeValue(mContext, exports);
    JSContext *context = mContext;
    addTickJob([this, context, tick]() {
        JSValue result = JS_Call(context, tick, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(result))
            logPendingException("<debug>");
        JS_FreeValue(context, result);
        return true;
    });
    mDebugTick = tick;
}
