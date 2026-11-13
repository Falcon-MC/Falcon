#include "Scripting/Binding/ScriptApi.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CameraAimAssistPacket.h"
#include "Scripting/AimAssistRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ScriptValues;

namespace {
    // ScriptPlayerAimAssist::set (sub_14B92E4A0, 26.60)
    const float DEFAULT_VIEW_ANGLE = 50.0f;
    const float MIN_VIEW_ANGLE = 10.0f;
    const float MAX_VIEW_ANGLE = 90.0f;
    const float DEFAULT_DISTANCE = 8.5f;
    const float MIN_DISTANCE = 1.0f;
    const float MAX_DISTANCE = 16.0f;
    // what an addPreset without them stores and sends (sub_14C94D040)
    const char *const FALLBACK_ITEM_SETTINGS = "default";
    const char *const FALLBACK_HAND_SETTINGS = "hand";
    // the packet a cleared aim assist sends (sub_142FC3670)
    const float CLEAR_VIEW_ANGLE_X = 30.0f;
    const float CLEAR_VIEW_ANGLE_Y = 45.0f;
    const float CLEAR_DISTANCE = 5.7f;

    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    AimAssistRegistry &registryOf(JSContext *ctx) {
        return ScriptApi::fromContext(ctx)->host().getScriptEngine().aimAssist();
    }

    // InvalidArgumentErrorType: Empty is 1, Unknown 5 (sub_148527430)
    JSValue invalidArgument(JSContext *ctx, const std::string &message, const char *type) {
        JSValue fields = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, fields, "type", JS_NewString(ctx, type));
        JS_SetPropertyStr(ctx, fields, "index", JS_NewInt32(ctx, 0));
        return ScriptApi::fromContext(ctx)->throwError("InvalidArgumentError", message, fields);
    }

    // sub_1468E7EF0
    JSValue invalidValue(JSContext *ctx, const std::string &argument, const char *type) {
        return invalidArgument(ctx, "Invalid provided value to argument '" + argument + "'.", type);
    }

    // sub_14C512410 with the minecraft namespace allowed: the namespace constraint (sub_1419A24C0) first, then one
    // namespace only when asked; NoNamespace is reason 1, DisallowedNamespace 2 (sub_14C514030)
    bool checkNamespace(JSContext *ctx, const std::string &name, bool single, JSValue &error) {
        const size_t colon = name.find(':');
        const char *reason = nullptr;
        const char *detail = nullptr;
        if (colon == std::string::npos || colon == 0 || colon + 1 >= name.size()) {
            reason = "NoNamespace";
            detail = "string must be prefixed with a namespace (eg. namespace:value)";
        } else if (single && name.find(':', colon + 1) != std::string::npos) {
            reason = "DisallowedNamespace";
            detail = "string cannot have multiple namespaces";
        }
        if (reason == nullptr)
            return true;
        JSValue fields = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, fields, "reason", JS_NewString(ctx, reason));
        // sub_14C512240
        error = ScriptApi::fromContext(ctx)->throwError(
                "NamespaceNameError", "There was an error with name '" + name + "': " + detail + ".", fields);
        return false;
    }

    // a category a preset names must exist (sub_14C89D280)
    bool checkCategory(JSContext *ctx, const std::string &id, JSValue &error) {
        if (!checkNamespace(ctx, id, false, error))
            return false;
        if (registryOf(ctx).findCategory(id) != nullptr)
            return true;
        error = invalidArgument(ctx, "Unknown category Id '" + id + "'.", "Unknown");
        return false;
    }

    bool readString(JSContext *ctx, JSValueConst value, std::string &out) {
        if (!JS_IsString(value))
            return false;
        out = toString(ctx, value);
        return true;
    }

    bool readStringList(JSContext *ctx, JSValueConst value, std::vector<std::string> &out) {
        if (!JS_IsArray(value))
            return false;
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, value, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        bool ok = true;
        for (uint32_t i = 0; ok && i < length; ++i) {
            JSValue entry = JS_GetPropertyUint32(ctx, value, i);
            std::string text;
            ok = readString(ctx, entry, text);
            if (ok)
                out.push_back(std::move(text));
            JS_FreeValue(ctx, entry);
        }
        return ok;
    }

    // a Record<string, T>, in key order; read converts one value and returns false to reject it
    template <typename T, typename Read>
    bool readRecord(JSContext *ctx, JSValueConst value, std::vector<std::pair<std::string, T>> &out, Read read) {
        if (!JS_IsObject(value) || JS_IsArray(value))
            return false;
        JSPropertyEnum *properties = nullptr;
        uint32_t count = 0;
        if (JS_GetOwnPropertyNames(ctx, &properties, &count, value, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) != 0)
            return false;
        bool ok = true;
        for (uint32_t i = 0; i < count && ok; i++) {
            JSValue entry = JS_GetProperty(ctx, value, properties[i].atom);
            const char *key = JS_AtomToCString(ctx, properties[i].atom);
            T converted{};
            ok = key != nullptr && read(entry, converted);
            if (ok)
                out.emplace_back(key, std::move(converted));
            if (key != nullptr)
                JS_FreeCString(ctx, key);
            JS_FreeValue(ctx, entry);
        }
        JS_FreePropertyEnum(ctx, properties, count);
        return ok;
    }

    bool readPriority(JSContext *ctx, JSValueConst value, int32_t &out) {
        return JS_IsNumber(value) && JS_ToInt32(ctx, &out, value) == 0;
    }

    bool readPriorities(JSContext *ctx, JSValueConst value, std::vector<std::pair<std::string, int32_t>> &out) {
        return readRecord<int32_t>(ctx, value, out, [ctx](JSValueConst entry, int32_t &priority) {
            return readPriority(ctx, entry, priority);
        });
    }

    JSValue priorityRecord(JSContext *ctx, const std::vector<std::pair<std::string, int32_t>> &priorities) {
        JSValue object = JS_NewObject(ctx);
        for (const auto &entry: priorities)
            JS_SetPropertyStr(ctx, object, entry.first.c_str(), JS_NewInt32(ctx, entry.second));
        return object;
    }

    JSValue stringRecord(JSContext *ctx, const std::vector<std::pair<std::string, std::string>> &entries) {
        JSValue object = JS_NewObject(ctx);
        for (const auto &entry: entries)
            JS_SetPropertyStr(ctx, object, entry.first.c_str(), string(ctx, entry.second));
        return object;
    }

    JSValue stringList(JSContext *ctx, const std::vector<std::string> &values) {
        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &value: values)
            JS_SetPropertyUint32(ctx, list, index++, string(ctx, value));
        return list;
    }

    JSValue optionalString(JSContext *ctx, const std::optional<std::string> &value) {
        return value.has_value() ? string(ctx, *value) : JS_UNDEFINED;
    }

    // AimAssistCategorySettings

    ScriptAimAssistCategorySettingsRef *categorySettingsOf(JSContext *ctx, JSValueConst value) {
        return ScriptApi::fromContext(ctx)->native<ScriptAimAssistCategorySettingsRef>(value);
    }

    JSValue categorySettingsConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptAimAssistCategorySettingsRef *ref = new ScriptAimAssistCategorySettingsRef();
        if (argc < 1 || !readString(ctx, argv[0], ref->mIdentifier)) {
            delete ref;
            return typeError(ctx);
        }
        return ScriptApi::fromContext(ctx)->makeInstance("AimAssistCategorySettings", ref);
    }

    JSValue categorySettingsGetIdentifier(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptAimAssistCategorySettingsRef *ref = categorySettingsOf(ctx, thisVal);
        return ref == nullptr ? typeError(ctx) : string(ctx, ref->mIdentifier);
    }

    // magic: 0 defaultEntityPriority, 1 defaultBlockPriority
    JSValue categorySettingsGetDefault(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const ScriptAimAssistCategorySettingsRef *ref = categorySettingsOf(ctx, thisVal);
        if (ref == nullptr)
            return typeError(ctx);
        return JS_NewInt32(ctx, magic == 0 ? ref->mDefaultEntityPriority : ref->mDefaultBlockPriority);
    }

    JSValue categorySettingsSetDefault(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptAimAssistCategorySettingsRef *ref = categorySettingsOf(ctx, thisVal);
        int32_t priority = 0;
        if (ref == nullptr || argc < 1 || !readPriority(ctx, argv[0], priority))
            return typeError(ctx);
        (magic == 0 ? ref->mDefaultEntityPriority : ref->mDefaultBlockPriority) = priority;
        return JS_UNDEFINED;
    }

    // magic: 0 entities, 1 blocks, 2 block tags, 3 entity type families
    std::vector<std::pair<std::string, int32_t>> &priorityList(ScriptAimAssistCategorySettingsRef &ref, int magic) {
        switch (magic) {
            case 0:
                return ref.mEntityPriorities;
            case 1:
                return ref.mBlockPriorities;
            case 2:
                return ref.mBlockTagPriorities;
            default:
                return ref.mEntityTypeFamilyPriorities;
        }
    }

    JSValue categorySettingsGetPriorities(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptAimAssistCategorySettingsRef *ref = categorySettingsOf(ctx, thisVal);
        return ref == nullptr ? typeError(ctx) : priorityRecord(ctx, priorityList(*ref, magic));
    }

    JSValue categorySettingsSetPriorities(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv,
                                          int magic) {
        ScriptAimAssistCategorySettingsRef *ref = categorySettingsOf(ctx, thisVal);
        std::vector<std::pair<std::string, int32_t>> priorities;
        if (ref == nullptr || argc < 1 || !readPriorities(ctx, argv[0], priorities))
            return typeError(ctx);
        priorityList(*ref, magic) = std::move(priorities);
        return JS_UNDEFINED;
    }

    // AimAssistPresetSettings

    ScriptAimAssistPresetSettingsRef *presetSettingsOf(JSContext *ctx, JSValueConst value) {
        return ScriptApi::fromContext(ctx)->native<ScriptAimAssistPresetSettingsRef>(value);
    }

    JSValue presetSettingsConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptAimAssistPresetSettingsRef *ref = new ScriptAimAssistPresetSettingsRef();
        if (argc < 1 || !readString(ctx, argv[0], ref->mIdentifier)) {
            delete ref;
            return typeError(ctx);
        }
        return ScriptApi::fromContext(ctx)->makeInstance("AimAssistPresetSettings", ref);
    }

    JSValue presetSettingsGetIdentifier(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptAimAssistPresetSettingsRef *ref = presetSettingsOf(ctx, thisVal);
        return ref == nullptr ? typeError(ctx) : string(ctx, ref->mIdentifier);
    }

    // magic: 0 defaultItemSettings, 1 handSettings
    JSValue presetSettingsGetCategory(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const ScriptAimAssistPresetSettingsRef *ref = presetSettingsOf(ctx, thisVal);
        if (ref == nullptr)
            return typeError(ctx);
        return optionalString(ctx, magic == 0 ? ref->mDefaultItemSettings : ref->mHandSettings);
    }

    JSValue presetSettingsSetCategory(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptAimAssistPresetSettingsRef *ref = presetSettingsOf(ctx, thisVal);
        std::string category;
        if (ref == nullptr || argc < 1 || (!JS_IsUndefined(argv[0]) && !readString(ctx, argv[0], category)))
            return typeError(ctx);
        std::optional<std::string> &target = magic == 0 ? ref->mDefaultItemSettings : ref->mHandSettings;
        if (JS_IsUndefined(argv[0]))
            target.reset();
        else
            target = category;
        return JS_UNDEFINED;
    }

    // magic: 0 excluded blocks, 1 excluded entities, 2 excluded block tags, 3 excluded entity type families,
    // 4 liquid targeting items
    std::optional<std::vector<std::string>> &targetList(ScriptAimAssistPresetSettingsRef &ref, int magic) {
        switch (magic) {
            case 0:
                return ref.mExcludedBlocks;
            case 1:
                return ref.mExcludedEntities;
            case 2:
                return ref.mExcludedBlockTags;
            case 3:
                return ref.mExcludedEntityTypeFamilies;
            default:
                return ref.mLiquidTargetingItems;
        }
    }

    JSValue presetSettingsGetList(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptAimAssistPresetSettingsRef *ref = presetSettingsOf(ctx, thisVal);
        if (ref == nullptr)
            return typeError(ctx);
        const std::optional<std::vector<std::string>> &list = targetList(*ref, magic);
        return list.has_value() ? stringList(ctx, *list) : JS_UNDEFINED;
    }

    JSValue presetSettingsSetList(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptAimAssistPresetSettingsRef *ref = presetSettingsOf(ctx, thisVal);
        if (ref == nullptr)
            return typeError(ctx);
        std::optional<std::vector<std::string>> &list = targetList(*ref, magic);
        if (argc < 1 || JS_IsUndefined(argv[0])) {
            list.reset();
            return JS_UNDEFINED;
        }
        std::vector<std::string> values;
        if (!readStringList(ctx, argv[0], values))
            return typeError(ctx);
        list = std::move(values);
        return JS_UNDEFINED;
    }

    JSValue presetSettingsGetItemSettings(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptAimAssistPresetSettingsRef *ref = presetSettingsOf(ctx, thisVal);
        return ref == nullptr ? typeError(ctx) : stringRecord(ctx, ref->mItemSettings);
    }

    JSValue presetSettingsSetItemSettings(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptAimAssistPresetSettingsRef *ref = presetSettingsOf(ctx, thisVal);
        std::vector<std::pair<std::string, std::string>> items;
        const bool ok = ref != nullptr && argc >= 1
                        && readRecord<std::string>(ctx, argv[0], items, [ctx](JSValueConst entry, std::string &out) {
                               return readString(ctx, entry, out);
                           });
        if (!ok)
            return typeError(ctx);
        ref->mItemSettings = std::move(items);
        return JS_UNDEFINED;
    }

    // AimAssistCategory and AimAssistPreset: the registry entry behind the handle

    const ScriptAimAssistRef *handleOf(JSContext *ctx, JSValueConst value) {
        return ScriptApi::fromContext(ctx)->native<ScriptAimAssistRef>(value);
    }

    JSValue makeHandle(JSContext *ctx, const char *className, const std::string &identifier) {
        ScriptAimAssistRef *ref = new ScriptAimAssistRef();
        ref->mIdentifier = identifier;
        return ScriptApi::fromContext(ctx)->makeInstance(className, ref);
    }

    JSValue handleGetIdentifier(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptAimAssistRef *ref = handleOf(ctx, thisVal);
        return ref == nullptr ? typeError(ctx) : string(ctx, ref->mIdentifier);
    }

    const AimAssistCategory *categoryOf(JSContext *ctx, JSValueConst thisVal) {
        const ScriptAimAssistRef *ref = handleOf(ctx, thisVal);
        return ref == nullptr ? nullptr : registryOf(ctx).findCategory(ref->mIdentifier);
    }

    // an unset default reads 1 (sub_14C88DB00)
    JSValue categoryGetDefault(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const AimAssistCategory *category = categoryOf(ctx, thisVal);
        if (category == nullptr)
            return typeError(ctx);
        return JS_NewInt32(
                ctx, (magic == 0 ? category->mDefaultEntityPriority : category->mDefaultBlockPriority).value_or(1));
    }

    JSValue categoryGetPriorities(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const AimAssistCategory *category = categoryOf(ctx, thisVal);
        if (category == nullptr)
            return typeError(ctx);
        switch (magic) {
            case 0:
                return priorityRecord(ctx, category->mEntityPriorities);
            case 1:
                return priorityRecord(ctx, category->mBlockPriorities);
            case 2:
                return priorityRecord(ctx, category->mBlockTagPriorities);
            default:
                return priorityRecord(ctx, category->mEntityTypeFamilyPriorities);
        }
    }

    const AimAssistPreset *presetOf(JSContext *ctx, JSValueConst thisVal) {
        const ScriptAimAssistRef *ref = handleOf(ctx, thisVal);
        return ref == nullptr ? nullptr : registryOf(ctx).findPreset(ref->mIdentifier);
    }

    JSValue presetGetCategory(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const AimAssistPreset *preset = presetOf(ctx, thisVal);
        if (preset == nullptr)
            return typeError(ctx);
        return optionalString(ctx, magic == 0 ? preset->mDefaultItemSettings : preset->mHandSettings);
    }

    JSValue presetGetList(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const AimAssistPreset *preset = presetOf(ctx, thisVal);
        if (preset == nullptr)
            return typeError(ctx);
        switch (magic) {
            case 0:
                return stringList(ctx, preset->mExcludedBlocks);
            case 1:
                return stringList(ctx, preset->mExcludedEntities);
            case 2:
                return stringList(ctx, preset->mExcludedBlockTags);
            case 3:
                return stringList(ctx, preset->mExcludedEntityTypeFamilies);
            default:
                return stringList(ctx, preset->mLiquidTargetingItems);
        }
    }

    JSValue presetGetItemSettings(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const AimAssistPreset *preset = presetOf(ctx, thisVal);
        return preset == nullptr ? typeError(ctx) : stringRecord(ctx, preset->mItemSettings);
    }

    // AimAssistRegistry

    // ScriptAimAssistRegistry::addCategory (sub_14C951720)
    JSValue registryAddCategory(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const ScriptAimAssistCategorySettingsRef *settings = argc < 1 ? nullptr : categorySettingsOf(ctx, argv[0]);
        if (settings == nullptr)
            return typeError(ctx);
        if (settings->mIdentifier.empty())
            return invalidValue(ctx, "categoryId", "Empty");
        JSValue error = JS_UNDEFINED;
        if (!checkNamespace(ctx, settings->mIdentifier, true, error))
            return error;

        AimAssistCategory category;
        category.mIdentifier = settings->mIdentifier;
        category.mEntityPriorities = settings->mEntityPriorities;
        category.mBlockPriorities = settings->mBlockPriorities;
        category.mBlockTagPriorities = settings->mBlockTagPriorities;
        category.mEntityTypeFamilyPriorities = settings->mEntityTypeFamilyPriorities;
        category.mDefaultEntityPriority = settings->mDefaultEntityPriority;
        category.mDefaultBlockPriority = settings->mDefaultBlockPriority;

        ScriptEngine &engine = ScriptApi::fromContext(ctx)->host().getScriptEngine();
        CameraAimAssistPresetsPacket packet;
        packet.mCategoryDefinitions.push_back(AimAssistRegistry::toPacket(category));
        packet.mOperation = CameraAimAssistOperation::AddToExisting;
        engine.aimAssist().addCategory(std::move(category));
        engine.broadcastAimAssist(packet);
        return makeHandle(ctx, "AimAssistCategory", settings->mIdentifier);
    }

    // ScriptAimAssistRegistry::addPreset (sub_14C94D040): the item categories, then the default, then the hand one
    JSValue registryAddPreset(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const ScriptAimAssistPresetSettingsRef *settings = argc < 1 ? nullptr : presetSettingsOf(ctx, argv[0]);
        if (settings == nullptr)
            return typeError(ctx);
        if (settings->mIdentifier.empty())
            return invalidValue(ctx, "presetId", "Empty");
        JSValue error = JS_UNDEFINED;
        if (!checkNamespace(ctx, settings->mIdentifier, true, error))
            return error;
        for (const auto &item: settings->mItemSettings) {
            if (!checkCategory(ctx, item.second, error))
                return error;
        }
        if (settings->mDefaultItemSettings.has_value() && !checkCategory(ctx, *settings->mDefaultItemSettings, error))
            return error;
        if (settings->mHandSettings.has_value() && !checkCategory(ctx, *settings->mHandSettings, error))
            return error;

        AimAssistPreset preset;
        preset.mIdentifier = settings->mIdentifier;
        preset.mExcludedBlocks = settings->mExcludedBlocks.value_or(std::vector<std::string>());
        preset.mExcludedEntities = settings->mExcludedEntities.value_or(std::vector<std::string>());
        preset.mExcludedBlockTags = settings->mExcludedBlockTags.value_or(std::vector<std::string>());
        preset.mExcludedEntityTypeFamilies = settings->mExcludedEntityTypeFamilies.value_or(std::vector<std::string>());
        preset.mLiquidTargetingItems = settings->mLiquidTargetingItems.value_or(std::vector<std::string>());
        preset.mItemSettings = settings->mItemSettings;
        preset.mDefaultItemSettings = settings->mDefaultItemSettings.value_or(FALLBACK_ITEM_SETTINGS);
        preset.mHandSettings = settings->mHandSettings.value_or(FALLBACK_HAND_SETTINGS);

        ScriptEngine &engine = ScriptApi::fromContext(ctx)->host().getScriptEngine();
        CameraAimAssistPresetsPacket packet;
        packet.mPresets.push_back(AimAssistRegistry::toPacket(preset));
        packet.mOperation = CameraAimAssistOperation::AddToExisting;
        engine.aimAssist().addPreset(std::move(preset));
        engine.broadcastAimAssist(packet);
        return makeHandle(ctx, "AimAssistPreset", settings->mIdentifier);
    }

    JSValue registryGetCategory(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        std::string id;
        if (argc < 1 || !readString(ctx, argv[0], id))
            return typeError(ctx);
        return registryOf(ctx).findCategory(id) == nullptr ? JS_UNDEFINED : makeHandle(ctx, "AimAssistCategory", id);
    }

    JSValue registryGetPreset(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        std::string id;
        if (argc < 1 || !readString(ctx, argv[0], id))
            return typeError(ctx);
        return registryOf(ctx).findPreset(id) == nullptr ? JS_UNDEFINED : makeHandle(ctx, "AimAssistPreset", id);
    }

    JSValue registryGetCategories(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const AimAssistCategory &category: registryOf(ctx).categories())
            JS_SetPropertyUint32(ctx, list, index++, makeHandle(ctx, "AimAssistCategory", category.mIdentifier));
        return list;
    }

    JSValue registryGetPresets(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const AimAssistPreset &preset: registryOf(ctx).presets())
            JS_SetPropertyUint32(ctx, list, index++, makeHandle(ctx, "AimAssistPreset", preset.mIdentifier));
        return list;
    }

    JSValue worldGetAimAssist(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return ScriptApi::fromContext(ctx)->makeInstance("AimAssistRegistry", new ScriptRegistryRef());
    }

    // PlayerAimAssist

    JSValue playerGetAimAssist(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptEntityRef *ref = api->native<ScriptEntityRef>(thisVal);
        if (ref == nullptr)
            return typeError(ctx);
        ScriptPlayerAimAssistRef *aimAssist = new ScriptPlayerAimAssistRef();
        aimAssist->mEntity = *ref;
        return api->makeInstance("PlayerAimAssist", aimAssist);
    }

    ServerPlayer *aimAssistOwner(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptPlayerAimAssistRef *ref = api->native<ScriptPlayerAimAssistRef>(thisVal);
        return ref == nullptr ? nullptr : dynamic_cast<ServerPlayer *>(api->resolveEntity(ref->mEntity));
    }

    JSValue aimAssistGetSettings(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = aimAssistOwner(ctx, thisVal);
        if (player == nullptr || !player->getAimAssist().has_value())
            return JS_UNDEFINED;
        const ServerPlayer::AimAssistSettings &settings = *player->getAimAssist();
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "presetId", string(ctx, settings.mPresetId));
        JS_SetPropertyStr(ctx, object, "targetMode", JS_NewString(ctx, settings.mDistanceMode ? "Distance" : "Angle"));
        JSValue viewAngle = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, viewAngle, "x", JS_NewFloat64(ctx, settings.mViewAngleX));
        JS_SetPropertyStr(ctx, viewAngle, "y", JS_NewFloat64(ctx, settings.mViewAngleY));
        JS_SetPropertyStr(ctx, object, "viewAngle", viewAngle);
        JS_SetPropertyStr(ctx, object, "distance", JS_NewFloat64(ctx, settings.mDistance));
        return object;
    }

    bool readOptionalNumber(JSContext *ctx, JSValueConst object, const char *name, std::optional<double> &out) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        bool ok = true;
        if (!JS_IsUndefined(value)) {
            double number = 0.0;
            ok = JS_IsNumber(value) && JS_ToFloat64(ctx, &number, value) == 0;
            out = number;
        }
        JS_FreeValue(ctx, value);
        return ok;
    }

    // PlayerAimAssistSettings as the binding converts it
    struct AimAssistRequest {
        std::string mPresetId;
        bool mDistanceMode = false;
        std::optional<double> mViewAngleX;
        std::optional<double> mViewAngleY;
        std::optional<double> mDistance;
    };

    bool readRequest(JSContext *ctx, JSValueConst object, AimAssistRequest &request) {
        if (!JS_IsObject(object))
            return false;
        JSValue presetId = JS_GetPropertyStr(ctx, object, "presetId");
        bool ok = readString(ctx, presetId, request.mPresetId);
        JS_FreeValue(ctx, presetId);

        JSValue mode = JS_GetPropertyStr(ctx, object, "targetMode");
        if (ok && !JS_IsUndefined(mode)) {
            std::string name;
            ok = readString(ctx, mode, name) && (name == "Angle" || name == "Distance");
            request.mDistanceMode = name == "Distance";
        }
        JS_FreeValue(ctx, mode);

        JSValue viewAngle = JS_GetPropertyStr(ctx, object, "viewAngle");
        if (ok && !JS_IsUndefined(viewAngle)) {
            ok = JS_IsObject(viewAngle) && readOptionalNumber(ctx, viewAngle, "x", request.mViewAngleX)
                 && readOptionalNumber(ctx, viewAngle, "y", request.mViewAngleY) && request.mViewAngleX.has_value()
                 && request.mViewAngleY.has_value();
        }
        JS_FreeValue(ctx, viewAngle);
        return ok && readOptionalNumber(ctx, object, "distance", request.mDistance);
    }

    // sub_148548380: argument 0, the member path and the bounds
    JSValue outOfBounds(JSContext *ctx, const char *member, double value, double minValue, double maxValue) {
        JSValue fields = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, fields, "index", JS_NewInt32(ctx, 0));
        JS_SetPropertyStr(ctx, fields, "minValue", JS_NewFloat64(ctx, minValue));
        JS_SetPropertyStr(ctx, fields, "maxValue", JS_NewFloat64(ctx, maxValue));
        JS_SetPropertyStr(ctx, fields, "value", JS_NewFloat64(ctx, value));
        return ScriptApi::fromContext(ctx)->throwError(
                "ArgumentOutOfBoundsError",
                std::string("Unsupported or out of bounds value passed to function argument [0]: ") + member
                        + ", Value: " + boundText(value) + ", Argument bounds: [" + boundText(minValue) + ", "
                        + boundText(maxValue) + "]",
                fields);
    }

    // ScriptPlayerAimAssist::set (sub_14B92E4A0): settings converted, then the player, then each value in order
    JSValue aimAssistSet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptPlayerAimAssistRef *ref = api->native<ScriptPlayerAimAssistRef>(thisVal);
        const bool clear = argc < 1 || JS_IsUndefined(argv[0]);
        AimAssistRequest request;
        if (ref == nullptr || (!clear && !readRequest(ctx, argv[0], request)))
            return typeError(ctx);
        ServerPlayer *player = dynamic_cast<ServerPlayer *>(api->resolveEntity(ref->mEntity));
        if (player == nullptr)
            return api->throwInvalidEntity("call function", "set", ref->mEntity);

        CameraAimAssistPacket packet;
        if (clear) {
            packet.mViewAngle.x = CLEAR_VIEW_ANGLE_X;
            packet.mViewAngle.y = CLEAR_VIEW_ANGLE_Y;
            packet.mDistance = CLEAR_DISTANCE;
            packet.mAction = AimAssistAction::Clear;
            player->getAimAssist().reset();
            api->host().sendPacketTo(player->getNetworkIdentifier(), packet);
            return JS_UNDEFINED;
        }

        if (request.mPresetId.empty())
            return invalidValue(ctx, "presetId", "Empty");
        JSValue error = JS_UNDEFINED;
        if (!checkNamespace(ctx, request.mPresetId, true, error))
            return error;
        const float angleX = (float) request.mViewAngleX.value_or(DEFAULT_VIEW_ANGLE);
        const float angleY = (float) request.mViewAngleY.value_or(DEFAULT_VIEW_ANGLE);
        if (angleX > MAX_VIEW_ANGLE || angleX < MIN_VIEW_ANGLE)
            return outOfBounds(ctx, "viewAngle.x", angleX, MIN_VIEW_ANGLE, MAX_VIEW_ANGLE);
        if (angleY > MAX_VIEW_ANGLE || angleY < MIN_VIEW_ANGLE)
            return outOfBounds(ctx, "viewAngle.y", angleY, MIN_VIEW_ANGLE, MAX_VIEW_ANGLE);
        const float distance = (float) request.mDistance.value_or(DEFAULT_DISTANCE);
        if (!(distance <= MAX_DISTANCE && distance >= MIN_DISTANCE))
            return outOfBounds(ctx, "distance", distance, MIN_DISTANCE, MAX_DISTANCE);
        // sub_142FC12B0: an unknown preset names itself as the argument
        if (registryOf(ctx).findPreset(request.mPresetId) == nullptr)
            return invalidValue(ctx, request.mPresetId, "Unknown");

        packet.mPresetId = request.mPresetId;
        packet.mViewAngle.x = angleX;
        packet.mViewAngle.y = angleY;
        packet.mDistance = distance;
        packet.mTargetMode = request.mDistanceMode ? CameraAimAssistPacket::TargetMode::Distance
                                                   : CameraAimAssistPacket::TargetMode::Angle;
        packet.mAction = AimAssistAction::Set;
        player->getAimAssist() = ServerPlayer::AimAssistSettings{request.mPresetId, request.mDistanceMode, angleX,
                                                                 angleY, distance};
        api->host().sendPacketTo(player->getNetworkIdentifier(), packet);
        return JS_UNDEFINED;
    }
}

void ScriptApi::_bindAimAssist() {
    if (mSurface.has("AimAssistCategorySettings")) {
        const char *cls = "AimAssistCategorySettings";
        JSValue settings = defineClass(cls, categorySettingsConstructor, 1);
        property(settings, cls, "identifier", categorySettingsGetIdentifier);
        propertyMagic(settings, cls, "defaultEntityPriority", categorySettingsGetDefault, 0,
                      categorySettingsSetDefault);
        propertyMagic(settings, cls, "defaultBlockPriority", categorySettingsGetDefault, 1, categorySettingsSetDefault);
        methodMagic(settings, cls, "getEntityPriorities", categorySettingsGetPriorities, 0, 0);
        methodMagic(settings, cls, "getBlockPriorities", categorySettingsGetPriorities, 0, 1);
        methodMagic(settings, cls, "getBlockTagPriorities", categorySettingsGetPriorities, 0, 2);
        methodMagic(settings, cls, "getEntityTypeFamilyPriorities", categorySettingsGetPriorities, 0, 3);
        methodMagic(settings, cls, "setEntityPriorities", categorySettingsSetPriorities, 1, 0);
        methodMagic(settings, cls, "setBlockPriorities", categorySettingsSetPriorities, 1, 1);
        methodMagic(settings, cls, "setBlockTagPriorities", categorySettingsSetPriorities, 1, 2);
        methodMagic(settings, cls, "setEntityTypeFamilyPriorities", categorySettingsSetPriorities, 1, 3);
    }

    if (mSurface.has("AimAssistPresetSettings")) {
        const char *cls = "AimAssistPresetSettings";
        JSValue settings = defineClass(cls, presetSettingsConstructor, 1);
        property(settings, cls, "identifier", presetSettingsGetIdentifier);
        propertyMagic(settings, cls, "defaultItemSettings", presetSettingsGetCategory, 0, presetSettingsSetCategory);
        propertyMagic(settings, cls, "handSettings", presetSettingsGetCategory, 1, presetSettingsSetCategory);
        methodMagic(settings, cls, "getExcludedBlockTargets", presetSettingsGetList, 0, 0);
        methodMagic(settings, cls, "getExcludedEntityTargets", presetSettingsGetList, 0, 1);
        methodMagic(settings, cls, "getExcludedBlockTagTargets", presetSettingsGetList, 0, 2);
        methodMagic(settings, cls, "getExcludedEntityTypeFamilyTargets", presetSettingsGetList, 0, 3);
        methodMagic(settings, cls, "getLiquidTargetingItems", presetSettingsGetList, 0, 4);
        methodMagic(settings, cls, "setExcludedBlockTargets", presetSettingsSetList, 1, 0);
        methodMagic(settings, cls, "setExcludedEntityTargets", presetSettingsSetList, 1, 1);
        methodMagic(settings, cls, "setExcludedBlockTagTargets", presetSettingsSetList, 1, 2);
        methodMagic(settings, cls, "setExcludedEntityTypeFamilyTargets", presetSettingsSetList, 1, 3);
        methodMagic(settings, cls, "setLiquidTargetingItems", presetSettingsSetList, 1, 4);
        method(settings, cls, "getItemSettings", presetSettingsGetItemSettings, 0);
        method(settings, cls, "setItemSettings", presetSettingsSetItemSettings, 1);
    }

    if (mSurface.has("AimAssistCategory")) {
        const char *cls = "AimAssistCategory";
        JSValue category = defineClass(cls);
        property(category, cls, "identifier", handleGetIdentifier);
        propertyMagic(category, cls, "defaultEntityPriority", categoryGetDefault, 0);
        propertyMagic(category, cls, "defaultBlockPriority", categoryGetDefault, 1);
        methodMagic(category, cls, "getEntityPriorities", categoryGetPriorities, 0, 0);
        methodMagic(category, cls, "getBlockPriorities", categoryGetPriorities, 0, 1);
        methodMagic(category, cls, "getBlockTagPriorities", categoryGetPriorities, 0, 2);
        methodMagic(category, cls, "getEntityTypeFamilyPriorities", categoryGetPriorities, 0, 3);
    }

    if (mSurface.has("AimAssistPreset")) {
        const char *cls = "AimAssistPreset";
        JSValue preset = defineClass(cls);
        property(preset, cls, "identifier", handleGetIdentifier);
        propertyMagic(preset, cls, "defaultItemSettings", presetGetCategory, 0);
        propertyMagic(preset, cls, "handSettings", presetGetCategory, 1);
        methodMagic(preset, cls, "getExcludedBlockTargets", presetGetList, 0, 0);
        methodMagic(preset, cls, "getExcludedEntityTargets", presetGetList, 0, 1);
        methodMagic(preset, cls, "getExcludedBlockTagTargets", presetGetList, 0, 2);
        methodMagic(preset, cls, "getExcludedEntityTypeFamilyTargets", presetGetList, 0, 3);
        methodMagic(preset, cls, "getLiquidTargetingItems", presetGetList, 0, 4);
        method(preset, cls, "getItemSettings", presetGetItemSettings, 0);
    }

    if (mSurface.has("AimAssistRegistry")) {
        const char *cls = "AimAssistRegistry";
        JSValue registry = defineClass(cls);
        method(registry, cls, "addCategory", registryAddCategory, 1);
        method(registry, cls, "addPreset", registryAddPreset, 1);
        method(registry, cls, "getCategory", registryGetCategory, 1);
        method(registry, cls, "getPreset", registryGetPreset, 1);
        method(registry, cls, "getCategories", registryGetCategories, 0);
        method(registry, cls, "getPresets", registryGetPresets, 0);
        JSValue constructor = constructorOf(cls);
        if (mSurface.has("AimAssistRegistry.static DefaultCategoryId"))
            JS_DefinePropertyValueStr(mContext, constructor, "DefaultCategoryId",
                                      JS_NewString(mContext, AimAssistRegistry::DEFAULT_CATEGORY), JS_PROP_ENUMERABLE);
        if (mSurface.has("AimAssistRegistry.static DefaultPresetId"))
            JS_DefinePropertyValueStr(mContext, constructor, "DefaultPresetId",
                                      JS_NewString(mContext, AimAssistRegistry::DEFAULT_PRESET), JS_PROP_ENUMERABLE);
        JSValue world = prototypeOf("World");
        if (!JS_IsUndefined(world))
            method(world, "World", "getAimAssist", worldGetAimAssist, 0);
    }

    if (mSurface.has("PlayerAimAssist")) {
        JSValue aimAssist = defineClass("PlayerAimAssist");
        property(aimAssist, "PlayerAimAssist", "settings", aimAssistGetSettings);
        method(aimAssist, "PlayerAimAssist", "set", aimAssistSet, 1);
        method(prototypeOf("Player"), "Player", "getAimAssist", playerGetAimAssist, 0);
    }
}
