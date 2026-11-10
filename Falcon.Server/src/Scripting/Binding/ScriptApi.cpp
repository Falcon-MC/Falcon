#include "Scripting/Binding/ScriptApi.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Server/Commands/CommandMap.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/Commands/PlayerCommandOrigin.h"
#include "Core/Debug/BedrockLog.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "Core/Event/EventBus.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Item/StringToItemParser.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Scripting/Binding/ScriptBootstraps.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/UpdateBlockPacket.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

using namespace ScriptValues;

namespace {
    const char *bindingLabel(ScriptApi::Binding::Kind kind) {
        switch (kind) {
            case ScriptApi::Binding::Kind::Getter:
                return "Native property getter";
            case ScriptApi::Binding::Kind::Setter:
                return "Native property setter";
            case ScriptApi::Binding::Kind::Constructor:
                return "Native constructor for";
            default:
                return "Native function";
        }
    }

    const char *bindingAction(ScriptApi::Binding::Kind kind) {
        switch (kind) {
            case ScriptApi::Binding::Kind::Getter:
                return "get property";
            case ScriptApi::Binding::Kind::Setter:
                return "set property";
            default:
                return "call function";
        }
    }

    void logException(JSContext *ctx, const std::string &pack, const std::string &where) {
        JSValue exception = JS_GetException(ctx);
        const std::string message = toString(ctx, exception);
        LOG_ERROR(LogAreaID::Script, "[%s] %s: %s", pack.c_str(), where.c_str(), message.c_str());

        JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
        if (JS_IsString(stack)) {
            const std::string trace = toString(ctx, stack);
            if (!trace.empty())
                LOG_ERROR(LogAreaID::Script, "%s", trace.c_str());
        }
        JS_FreeValue(ctx, stack);
        JS_FreeValue(ctx, exception);
    }

    template <typename T>
    bool accepts(const ScriptNative *native) {
        return dynamic_cast<const T *>(native) != nullptr;
    }

    bool acceptsPlayer(const ScriptNative *native) {
        const ScriptEntityRef *ref = dynamic_cast<const ScriptEntityRef *>(native);
        return ref != nullptr && ref->mKind == ScriptEntityRef::Kind::Player;
    }

    bool acceptsComponent(const ScriptNative *native) {
        return accepts<ScriptEntityPart>(native) || accepts<ScriptPlayerPart>(native) || accepts<ScriptItemRef>(native);
    }

    bool acceptsAny(const ScriptNative *) {
        return true;
    }

    bool hasSuffix(const std::string &text, const char *suffix) {
        const size_t length = std::strlen(suffix);
        return text.size() >= length && text.compare(text.size() - length, length, suffix) == 0;
    }

    bool (*acceptorFor(const std::string &cls))(const ScriptNative *) {
        if (hasSuffix(cls, "Signal"))
            return accepts<ScriptSignalRef>;
        if (hasSuffix(cls, "Registry"))
            return accepts<ScriptRegistryRef>;
        if (cls == "Entity")
            return accepts<ScriptEntityRef>;
        if (cls == "Player")
            return acceptsPlayer;
        if (cls == "Block")
            return accepts<ScriptBlockRef>;
        if (cls == "Dimension")
            return accepts<ScriptDimensionRef>;
        if (cls == "Component")
            return acceptsComponent;
        if (cls == "ItemStack" || cls.compare(0, 4, "Item") == 0)
            return accepts<ScriptItemRef>;
        if (cls == "EntityAttributeComponent" || cls == "EntityHealthComponent" || cls == "EntityTypeFamilyComponent"
            || cls == "EntityProjectileComponent")
            return accepts<ScriptEntityPart>;
        if (cls == "ScreenDisplay" || cls == "Camera" || cls == "PlayerInputPermissions" || cls == "LocatorBar"
            || cls == "Container" || cls == "ContainerSlot" || cls == "EntityInventoryComponent"
            || cls == "EntityEquippableComponent")
            return accepts<ScriptPlayerPart>;
        return acceptsAny;
    }

    // native dispatch: private constructor, privilege, bound object, live entity, then the native
    JSValue bindingTrampoline(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptApi::Binding &binding = api->binding(magic);
        const char *label = bindingLabel(binding.mKind);

        if (binding.mKind == ScriptApi::Binding::Kind::Constructor && binding.mFunction == nullptr) {
            const std::string className = binding.mName.substr(0, binding.mName.find(':'));
            return JS_ThrowReferenceError(ctx, "No constructor for native class '%s'.", className.c_str());
        }
        if (api->privilege() == ScriptPrivilege::Early && !binding.mEarly)
            return JS_ThrowReferenceError(ctx, "%s [%s] cannot be used in early execution.", label,
                                          binding.mName.c_str());
        if (api->privilege() == ScriptPrivilege::Restricted && binding.mBlocked)
            return JS_ThrowReferenceError(ctx, "%s [%s] cannot be used in restricted execution.", label,
                                          binding.mName.c_str());

        if (binding.mKind == ScriptApi::Binding::Kind::Constructor)
            return binding.mFunction(ctx, thisVal, argc, argv);

        const ScriptNative *native = api->native<ScriptNative>(thisVal);
        if (native == nullptr || !binding.mAccepts(native))
            return JS_ThrowReferenceError(ctx, "%s [%s] object bound to prototype does not exist.", label,
                                          binding.mName.c_str());

        if (binding.mEntity) {
            const ScriptEntityRef *ref = api->native<ScriptEntityRef>(thisVal);
            const std::string member = binding.mName.substr(binding.mName.rfind(':') + 1);
            if (api->resolveEntity(*ref) == nullptr && member != "isValid")
                return api->throwInvalidEntity(bindingAction(binding.mKind), member, *ref);
        }

        if (binding.mMagicFunction != nullptr)
            return binding.mMagicFunction(ctx, thisVal, argc, argv, binding.mMagic);
        return binding.mFunction(ctx, thisVal, argc, argv);
    }

    JSValue signalSubscribe(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptSignalRef *signal = api->native<ScriptSignalRef>(thisVal);
        if (signal == nullptr || argc < 1 || !JS_IsFunction(ctx, argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        api->addSubscriber(signal->mKey, JS_DupValue(ctx, argv[0]));
        if (signal->mKey == ScriptApi::SCRIPT_EVENT_KEY && argc >= 2 && JS_IsObject(argv[1])) {
            JSValue list = JS_GetPropertyStr(ctx, argv[1], "namespaces");
            if (JS_IsArray(list)) {
                std::vector<std::string> namespaces;
                uint32_t length = 0;
                JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
                JS_ToUint32(ctx, &length, lengthValue);
                JS_FreeValue(ctx, lengthValue);
                for (uint32_t index = 0; index < length; ++index) {
                    JSValue item = JS_GetPropertyUint32(ctx, list, index);
                    namespaces.push_back(toString(ctx, item));
                    JS_FreeValue(ctx, item);
                }
                api->filterScriptEvents(argv[0], std::move(namespaces));
            }
            JS_FreeValue(ctx, list);
        }
        return JS_DupValue(ctx, argv[0]);
    }

    JSValue signalUnsubscribe(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptSignalRef *signal = api->native<ScriptSignalRef>(thisVal);
        if (signal != nullptr && argc >= 1)
            api->removeSubscriber(signal->mKey, argv[0]);
        return JS_UNDEFINED;
    }

    JSValue registryRegister(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptRegistryRef *registry = api->native<ScriptRegistryRef>(thisVal);
        if (registry == nullptr || argc < 2)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        // ScriptBlockComponentRegistry: registration closes with the startup event
        if (!api->registerCustomComponent(registry->mBlocks, toString(ctx, argv[0]), argv[1]))
            return api->throwError("CustomComponentInvalidRegistryError",
                                   "Cannot register custom component outside of the ModuleStartup event.");
        return JS_UNDEFINED;
    }

    JSValue stateValueToJs(JSContext *ctx, const Tag &value) {
        switch (value.getType()) {
            case Tag::Type::Byte:
                return JS_NewBool(ctx, value.asByte() != 0);
            case Tag::Type::Short:
                return JS_NewInt32(ctx, value.asShort());
            case Tag::Type::Int:
                return JS_NewInt32(ctx, value.asInt());
            case Tag::Type::String:
                return string(ctx, value.asString());
            default:
                return JS_UNDEFINED;
        }
    }

    JSValue paletteBlockNames(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue names = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &name: BlockPaletteRegistry::getInstance().getBlockNames())
            JS_SetPropertyUint32(ctx, names, index++, string(ctx, name));
        return names;
    }

    JSValue palettePermutations(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;

        const std::vector<Tag> *permutations = BlockPaletteRegistry::getInstance().getPermutations(
                toString(ctx, argv[0]));
        if (permutations == nullptr)
            return JS_UNDEFINED;

        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const Tag &states: *permutations) {
            JSValue object = JS_NewObject(ctx);
            const std::vector<std::string> &keys = states.getKeys();
            const std::vector<Tag> &values = states.getValues();
            for (size_t key = 0; key < keys.size(); ++key)
                JS_SetPropertyStr(ctx, object, keys[key].c_str(), stateValueToJs(ctx, values[key]));
            JS_SetPropertyUint32(ctx, list, index++, object);
        }
        return list;
    }

    JSValue itemTypeNames(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        JSValue names = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::shared_ptr<ItemDefinition> &definition: api->host().getItemDefinitions().getAll())
            JS_SetPropertyUint32(ctx, names, index++, string(ctx, definition->getIdentifier()));
        return names;
    }

    JSValue uiShowForm(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 2)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return api->showForm(argv[0], toString(ctx, argv[1]));
    }

    JSValue scoreboardSetScore(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 3)
            return JS_UNDEFINED;
        int32_t score = 0;
        JS_ToInt32(ctx, &score, argv[2]);
        api->host().setScoreboardScore(toString(ctx, argv[0]), toString(ctx, argv[1]), score);
        return JS_UNDEFINED;
    }

    JSValue scoreboardRemoveScore(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc >= 2)
            api->host().removeScoreboardScore(toString(ctx, argv[0]), toString(ctx, argv[1]));
        return JS_UNDEFINED;
    }

    JSValue scoreboardDisplay(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 2)
            return JS_UNDEFINED;
        const std::string displayName = argc >= 3 ? toString(ctx, argv[2]) : std::string();
        api->host().displayScoreboardObjective(toString(ctx, argv[0]), toString(ctx, argv[1]), displayName);
        return JS_UNDEFINED;
    }

    JSValue scoreboardClearDisplay(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc >= 1)
            api->host().clearScoreboardDisplay(toString(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue scoreboardRemoveObjective(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc >= 1)
            api->host().removeScoreboardObjective(toString(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue runBootstrap(JSContext *ctx, const std::string &pack, const char *source, const char *name,
                         JSValue argument) {
        JSValue factory = JS_Eval(ctx, source, std::strlen(source), name, JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(factory)) {
            logException(ctx, pack, name);
            JS_FreeValue(ctx, argument);
            return JS_UNDEFINED;
        }

        JSValue result = JS_Call(ctx, factory, JS_UNDEFINED, 1, &argument);
        JS_FreeValue(ctx, factory);
        JS_FreeValue(ctx, argument);
        if (JS_IsException(result)) {
            logException(ctx, pack, name);
            return JS_UNDEFINED;
        }
        return result;
    }

    void addFunction(JSContext *ctx, JSValue object, const char *name, JSCFunction *function, int length) {
        JS_SetPropertyStr(ctx, object, name, JS_NewCFunction(ctx, function, name, length));
    }

    int moduleInit(JSContext *ctx, JSModuleDef *module);

    const char *const SERVER_ENUMS[] = {"EquipmentSlot",
                                        "GameMode",
                                        "InputPermissionCategory",
                                        "Direction",
                                        "PlayerInventoryType",
                                        "EntityComponentTypes",
                                        "ItemComponentTypes",
                                        "BlockComponentTypes",
                                        "Difficulty",
                                        "TimeOfDay",
                                        "MoonPhase",
                                        "WeatherType",
                                        "ScriptEventSource",
                                        "GameRule",
                                        "MemoryTier"};

    const char *const COMPONENT_IDS[][2] = {
            {"EntityInventoryComponent", "minecraft:inventory"},
            {"EntityEquippableComponent", "minecraft:equippable"},
            {"EntityHealthComponent", "minecraft:health"},
            {"EntityTypeFamilyComponent", "minecraft:type_family"},
            {"EntityProjectileComponent", "minecraft:projectile"},
            {"ItemDurabilityComponent", "minecraft:durability"},
            {"ItemEnchantableComponent", "minecraft:enchantable"},
            {"ItemCooldownComponent", "minecraft:cooldown"},
    };

    bool isNumber(const std::string &text) {
        if (text.empty())
            return false;
        for (const char character: text) {
            if (character < '0' || character > '9')
                return false;
        }
        return true;
    }

    ScriptModuleVersion moduleOrDefault(const ScriptModuleSet &modules, const char *name) {
        const ScriptModuleVersion *found = modules.find(name);
        if (found != nullptr)
            return *found;

        // ui and common follow the server's major when a pack does not pin them
        ScriptModuleVersion fallback;
        const ScriptModuleVersion *server = modules.find("@minecraft/server");
        if (server != nullptr)
            fallback.mTrack = server->mTrack;
        fallback.mMinor = 0;
        return fallback;
    }
}

ScriptApi::ScriptApi(JSContext *context, ServerNetworkHandler &host, PlayerHandleRegistry &players,
                     JSClassID nativeClassId, std::string packName, std::string packRoot, ScriptModuleSet modules)
        : mContext(context), mHost(host), mNativeClassId(nativeClassId), mPackName(std::move(packName)),
          mPackRoot(std::move(packRoot)), mModules(std::move(modules)),
          mTrack(moduleOrDefault(mModules, "@minecraft/server").mTrack),
          mSurface("@minecraft/server", moduleOrDefault(mModules, "@minecraft/server")),
          mUiSurface("@minecraft/server-ui", moduleOrDefault(mModules, "@minecraft/server-ui")),
          mCommonSurface("@minecraft/common", moduleOrDefault(mModules, "@minecraft/common")), mScheduler(context),
          mPlayers(players) {
    JS_SetContextOpaque(mContext, this);
}

ScriptApi::~ScriptApi() {
    mScheduler.clearAll();

    for (auto &entry: mSubscribers) {
        for (JSValue callback: entry.second)
            JS_FreeValue(mContext, callback);
    }
    for (auto &entry: mBlockComponents)
        JS_FreeValue(mContext, entry.second);
    for (auto &entry: mItemComponents)
        JS_FreeValue(mContext, entry.second);
    for (auto &entry: mFormResolvers)
        JS_FreeValue(mContext, entry.second);
    for (auto &entry: mActorObjects)
        JS_FreeValue(mContext, entry.second);
    for (auto &entry: mPlayerObjects)
        JS_FreeValue(mContext, entry.second);
    for (auto &entry: mDimensionObjects)
        JS_FreeValue(mContext, entry.second);
    for (auto *exports: {&mServerExports, &mUiExports, &mCommonExports}) {
        for (auto &entry: *exports)
            JS_FreeValue(mContext, entry.second);
    }
    for (auto &entry: mClasses) {
        JS_FreeValue(mContext, entry.second.mConstructor);
        JS_FreeValue(mContext, entry.second.mPrototype);
    }

    for (JSValue value: {mWorld, mSystem, mAfterEvents, mBeforeEvents, mSystemAfterEvents, mSystemBeforeEvents,
                         mScoreboard, mTickingAreas, mGameRules, mSystemInfo, mErrors, mBlockRegistry, mItemRegistry})
        JS_FreeValue(mContext, value);
}

ScriptApi *ScriptApi::fromContext(JSContext *ctx) {
    return (ScriptApi *) JS_GetContextOpaque(ctx);
}

int ScriptApi::_addBinding(Binding binding) {
    mBindings.push_back(std::move(binding));
    return (int) mBindings.size() - 1;
}

bool ScriptApi::method(JSValueConst target, const char *cls, const char *name, JSCFunction *function, int length) {
    const std::string key = std::string(cls) + "." + name;
    if (!mSurface.has(key))
        return false;

    Binding binding;
    binding.mName = std::string(cls) + "::" + name;
    binding.mAccepts = acceptorFor(cls);
    binding.mFunction = function;
    binding.mEarly = mSurface.isEarly(key);
    binding.mBlocked = mSurface.isBlocked(key);
    binding.mEntity = std::strcmp(cls, "Entity") == 0 || std::strcmp(cls, "Player") == 0;
    const int index = _addBinding(binding);
    JS_SetPropertyStr(mContext, target, name,
                      JS_NewCFunctionMagic(mContext, bindingTrampoline, name, length, JS_CFUNC_generic_magic, index));
    return true;
}

bool ScriptApi::methodMagic(JSValueConst target, const char *cls, const char *name, JSCFunctionMagic *function,
                            int length, int magic) {
    const std::string key = std::string(cls) + "." + name;
    if (!mSurface.has(key))
        return false;

    Binding binding;
    binding.mName = std::string(cls) + "::" + name;
    binding.mAccepts = acceptorFor(cls);
    binding.mMagicFunction = function;
    binding.mMagic = magic;
    binding.mEarly = mSurface.isEarly(key);
    binding.mBlocked = mSurface.isBlocked(key);
    binding.mEntity = std::strcmp(cls, "Entity") == 0 || std::strcmp(cls, "Player") == 0;
    const int index = _addBinding(binding);
    JS_SetPropertyStr(mContext, target, name,
                      JS_NewCFunctionMagic(mContext, bindingTrampoline, name, length, JS_CFUNC_generic_magic, index));
    return true;
}

void ScriptApi::signalMethod(JSValueConst target, const char *cls, const char *name, JSCFunction *function) {
    const std::string key = std::string(cls) + "." + name;

    Binding binding;
    binding.mName = std::string(cls) + "::" + name;
    binding.mAccepts = acceptorFor(cls);
    binding.mFunction = function;
    binding.mEarly = !mSurface.has(key) || mSurface.isEarly(key);
    binding.mBlocked = !mSurface.has(key) || mSurface.isBlocked(key);
    const int index = _addBinding(binding);
    JS_SetPropertyStr(mContext, target, name,
                      JS_NewCFunctionMagic(mContext, bindingTrampoline, name, 1, JS_CFUNC_generic_magic, index));
}

bool ScriptApi::property(JSValueConst target, const char *cls, const char *name, JSCFunction *getter,
                         JSCFunction *setter) {
    const std::string key = std::string(cls) + "." + name;
    if (!mSurface.has(key))
        return false;

    const bool entity = std::strcmp(cls, "Entity") == 0 || std::strcmp(cls, "Player") == 0;

    Binding read;
    read.mName = std::string(cls) + "::" + name;
    read.mKind = Binding::Kind::Getter;
    read.mAccepts = acceptorFor(cls);
    read.mFunction = getter;
    read.mEarly = mSurface.isEarly(key);
    read.mEntity = entity;
    JSValue getterValue = JS_NewCFunctionMagic(mContext, bindingTrampoline, name, 0, JS_CFUNC_generic_magic,
                                               _addBinding(read));

    JSValue setterValue = JS_UNDEFINED;
    if (setter != nullptr) {
        Binding write = read;
        write.mKind = Binding::Kind::Setter;
        write.mFunction = setter;
        write.mEarly = false;
        write.mBlocked = mSurface.isBlocked(key);
        setterValue = JS_NewCFunctionMagic(mContext, bindingTrampoline, name, 1, JS_CFUNC_generic_magic,
                                           _addBinding(write));
    }

    JSAtom atom = JS_NewAtom(mContext, name);
    JS_DefinePropertyGetSet(mContext, target, atom, getterValue, setterValue,
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(mContext, atom);
    return true;
}

bool ScriptApi::propertyMagic(JSValueConst target, const char *cls, const char *name, JSCFunctionMagic *getter,
                              int magic, JSCFunctionMagic *setter) {
    const std::string key = std::string(cls) + "." + name;
    if (!mSurface.has(key))
        return false;

    Binding read;
    read.mName = std::string(cls) + "::" + name;
    read.mKind = Binding::Kind::Getter;
    read.mAccepts = acceptorFor(cls);
    read.mMagicFunction = getter;
    read.mMagic = magic;
    read.mEarly = mSurface.isEarly(key);
    read.mEntity = std::strcmp(cls, "Entity") == 0 || std::strcmp(cls, "Player") == 0;
    JSValue getterValue = JS_NewCFunctionMagic(mContext, bindingTrampoline, name, 0, JS_CFUNC_generic_magic,
                                               _addBinding(read));

    JSValue setterValue = JS_UNDEFINED;
    if (setter != nullptr) {
        Binding write = read;
        write.mKind = Binding::Kind::Setter;
        write.mMagicFunction = setter;
        write.mEarly = false;
        write.mBlocked = mSurface.isBlocked(key);
        setterValue = JS_NewCFunctionMagic(mContext, bindingTrampoline, name, 1, JS_CFUNC_generic_magic,
                                           _addBinding(write));
    }

    JSAtom atom = JS_NewAtom(mContext, name);
    JS_DefinePropertyGetSet(mContext, target, atom, getterValue, setterValue,
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(mContext, atom);
    return true;
}

JSValue ScriptApi::defineClass(const std::string &name, JSCFunction *constructor, int length) {
    const auto existing = mClasses.find(name);
    if (existing != mClasses.end())
        return existing->second.mPrototype;

    JSValue parentPrototype = JS_UNDEFINED;
    JSValue parentConstructor = JS_UNDEFINED;
    const char *parent = ScriptSurface::findParent(name);
    if (parent != nullptr && mSurface.has(parent)) {
        parentPrototype = defineClass(parent);
        parentConstructor = constructorOf(parent);
    }

    Binding binding;
    binding.mName = name + "::constructor";
    binding.mKind = Binding::Kind::Constructor;
    binding.mFunction = mSurface.has(name + ".constructor") ? constructor : nullptr;
    binding.mEarly = mSurface.isEarly(name + ".constructor");
    binding.mBlocked = mSurface.isBlocked(name + ".constructor");
    const int index = _addBinding(binding);

    ScriptClass created;
    created.mConstructor = JS_NewCFunctionMagic(mContext, bindingTrampoline, name.c_str(), length,
                                                JS_CFUNC_constructor_or_func_magic, index);
    created.mPrototype = JS_IsUndefined(parentPrototype) ? JS_NewObject(mContext)
                                                         : JS_NewObjectProto(mContext, parentPrototype);
    JS_SetConstructor(mContext, created.mConstructor, created.mPrototype);
    if (!JS_IsUndefined(parentConstructor))
        JS_SetPrototype(mContext, created.mConstructor, parentConstructor);

    mClasses[name] = created;
    if (mSurface.has(name))
        mServerExports.emplace_back(name, JS_DupValue(mContext, created.mConstructor));
    return created.mPrototype;
}

JSValue ScriptApi::prototypeOf(const std::string &className) const {
    const auto it = mClasses.find(className);
    return it == mClasses.end() ? JS_UNDEFINED : it->second.mPrototype;
}

JSValue ScriptApi::constructorOf(const std::string &className) const {
    const auto it = mClasses.find(className);
    return it == mClasses.end() ? JS_UNDEFINED : it->second.mConstructor;
}

JSValue ScriptApi::makeInstance(const std::string &className, ScriptNative *data) {
    JSValue prototype = prototypeOf(className);
    if (JS_IsUndefined(prototype))
        prototype = defineClass(className);

    JSValue object = JS_NewObjectProtoClass(mContext, prototype, mNativeClassId);
    JS_SetOpaque(object, data);
    return object;
}

JSValue ScriptApi::makeEvent(const std::string &signalKey) {
    const ScriptSignalEntry *signal = ScriptSurface::findSignal(signalKey);
    if (signal == nullptr || signal->mEventClass[0] == '\0')
        return JS_NewObject(mContext);

    JSValue prototype = prototypeOf(signal->mEventClass);
    if (JS_IsUndefined(prototype))
        return JS_NewObject(mContext);
    return JS_NewObjectProto(mContext, prototype);
}

JSValue ScriptApi::throwError(const std::string &errorClass, const std::string &message, JSValue fields) {
    JSValue make = JS_GetPropertyStr(mContext, mErrors, "make");
    JSValue args[3] = {string(mContext, errorClass), string(mContext, message), fields};
    JSValue error = JS_Call(mContext, make, JS_UNDEFINED, 3, args);
    for (JSValue arg: args)
        JS_FreeValue(mContext, arg);
    JS_FreeValue(mContext, make);
    if (JS_IsException(error))
        return JS_EXCEPTION;
    return JS_Throw(mContext, error);
}

JSValue ScriptApi::throwInvalidEntity(const char *action, const std::string &member, const ScriptEntityRef &entity) {
    const std::string message = std::string("Failed to ") + action + " '" + member
                                + "' due to Entity being invalid (has the Entity been removed?).";
    JSValue make = JS_GetPropertyStr(mContext, mErrors, "make");
    JSValue fields = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, fields, "id", string(mContext, entity.mEntityId));
    JS_SetPropertyStr(mContext, fields, "type", string(mContext, entity.mTypeId));
    JSValue args[3] = {string(mContext, "InvalidEntityError"), string(mContext, message), fields};
    JSValue error = JS_Call(mContext, make, JS_UNDEFINED, 3, args);
    for (JSValue arg: args)
        JS_FreeValue(mContext, arg);
    JS_FreeValue(mContext, make);
    if (JS_IsException(error))
        return JS_EXCEPTION;
    return JS_Throw(mContext, error);
}

ServerPlayer *ScriptApi::resolvePlayerByHandle(uint32_t handle) {
    const NetworkIdentifier *id = mPlayers.lookup(handle);
    if (id == nullptr)
        return nullptr;

    std::unordered_map<NetworkIdentifier, ServerPlayer, NetworkIdentifier::Hasher> &players = mHost.getPlayers();
    auto it = players.find(*id);
    return it == players.end() ? nullptr : &it->second;
}

ServerPlayer *ScriptApi::resolvePlayer(JSValueConst value) {
    const ScriptEntityRef *ref = native<ScriptEntityRef>(value);
    if (ref == nullptr || ref->mKind != ScriptEntityRef::Kind::Player)
        return nullptr;
    return resolvePlayerByHandle((uint32_t) ref->mId);
}

ServerActor *ScriptApi::resolveActor(JSValueConst value) {
    const ScriptEntityRef *ref = native<ScriptEntityRef>(value);
    if (ref == nullptr || ref->mKind != ScriptEntityRef::Kind::Actor)
        return nullptr;
    return mHost.getActor(ref->mId);
}

Actor *ScriptApi::resolveEntity(const ScriptEntityRef &ref) {
    switch (ref.mKind) {
        case ScriptEntityRef::Kind::Player:
            return resolvePlayerByHandle((uint32_t) ref.mId);
        case ScriptEntityRef::Kind::Actor:
            return mHost.getActor(ref.mId);
        case ScriptEntityRef::Kind::Item:
            for (const std::unique_ptr<ItemActor> &item: mHost.getItemEntities()) {
                if ((int64_t) item->getRuntimeId() == ref.mId && !item->isRemoved())
                    return item.get();
            }
            return nullptr;
    }
    return nullptr;
}

Actor *ScriptApi::resolveEntity(JSValueConst value) {
    const ScriptEntityRef *ref = native<ScriptEntityRef>(value);
    return ref == nullptr ? nullptr : resolveEntity(*ref);
}

JSValue ScriptApi::makePlayer(ServerPlayer &player) {
    const uint32_t handle = mPlayers.acquire(player.getNetworkIdentifier());
    const auto cached = mPlayerObjects.find(handle);
    if (cached != mPlayerObjects.end())
        return JS_DupValue(mContext, cached->second);

    ScriptEntityRef *ref = new ScriptEntityRef();
    ref->mKind = ScriptEntityRef::Kind::Player;
    ref->mId = handle;
    ref->mEntityId = std::to_string(player.getUniqueId());
    ref->mTypeId = "minecraft:player";
    JSValue object = makeInstance("Player", ref);
    mPlayerObjects[handle] = JS_DupValue(mContext, object);
    return object;
}

JSValue ScriptApi::makeActor(ServerActor &actor) {
    const auto cached = mActorObjects.find(actor.getUniqueId());
    if (cached != mActorObjects.end())
        return JS_DupValue(mContext, cached->second);

    ScriptEntityRef *ref = new ScriptEntityRef();
    ref->mKind = ScriptEntityRef::Kind::Actor;
    ref->mId = actor.getUniqueId();
    ref->mEntityId = std::to_string(actor.getUniqueId());
    ref->mTypeId = actor.getTypeId();
    JSValue object = makeInstance("Entity", ref);
    mActorObjects[actor.getUniqueId()] = JS_DupValue(mContext, object);
    return object;
}

JSValue ScriptApi::makeEntity(Actor &actor) {
    ServerPlayer *player = dynamic_cast<ServerPlayer *>(&actor);
    if (player != nullptr)
        return makePlayer(*player);

    ServerActor *serverActor = dynamic_cast<ServerActor *>(&actor);
    if (serverActor != nullptr)
        return makeActor(*serverActor);

    ItemActor *item = dynamic_cast<ItemActor *>(&actor);
    if (item == nullptr)
        return JS_UNDEFINED;

    ScriptEntityRef *ref = new ScriptEntityRef();
    ref->mKind = ScriptEntityRef::Kind::Item;
    ref->mId = (int64_t) item->getRuntimeId();
    ref->mEntityId = std::to_string(item->getUniqueId());
    ref->mTypeId = ItemActor::IDENTIFIER;
    return makeInstance("Entity", ref);
}

void ScriptApi::forgetEntity(int64_t uniqueId) {
    const auto it = mActorObjects.find(uniqueId);
    if (it == mActorObjects.end())
        return;
    JS_FreeValue(mContext, it->second);
    mActorObjects.erase(it);
}

void ScriptApi::forgetPlayer(ServerPlayer &player) {
    const uint32_t handle = mPlayers.acquire(player.getNetworkIdentifier());
    const auto it = mPlayerObjects.find(handle);
    if (it == mPlayerObjects.end())
        return;
    JS_FreeValue(mContext, it->second);
    mPlayerObjects.erase(it);
}

Level &ScriptApi::resolveLevel(uint32_t worldId, DimensionType dimension) {
    World *world = mHost.getWorlds().find(worldId);
    return (world == nullptr ? mHost.getWorlds().getDefault() : *world).getLevel(dimension);
}

Level &ScriptApi::levelOf(JSValueConst dimension) {
    const ScriptDimensionRef *ref = native<ScriptDimensionRef>(dimension);
    if (ref == nullptr)
        return mHost.getWorlds().getDefault().getLevel(DimensionType::Overworld);
    return resolveLevel(ref->mWorldId, ref->mDimension);
}

JSValue ScriptApi::makeDimension(const Level &level) {
    const uint64_t key = ((uint64_t) level.getWorldId() << 8) | (uint64_t) level.getDimensionType();
    const auto cached = mDimensionObjects.find(key);
    if (cached != mDimensionObjects.end())
        return JS_DupValue(mContext, cached->second);

    ScriptDimensionRef *ref = new ScriptDimensionRef();
    ref->mWorldId = level.getWorldId();
    ref->mDimension = level.getDimensionType();
    JSValue object = makeInstance("Dimension", ref);
    mDimensionObjects[key] = JS_DupValue(mContext, object);
    return object;
}

JSValue ScriptApi::makeBlock(const Level &level, int32_t x, int32_t y, int32_t z) {
    ScriptBlockRef *ref = new ScriptBlockRef();
    ref->mDimension = level.getDimensionType();
    ref->mWorldId = level.getWorldId();
    ref->mX = x;
    ref->mY = y;
    ref->mZ = z;
    return makeInstance("Block", ref);
}

JSValue ScriptApi::makeItemStack(const std::string &typeId, int32_t amount) {
    ScriptItemRef *ref = new ScriptItemRef();
    ref->mItem = std::make_shared<ScriptItemData>();
    ref->mItem->mTypeId = typeId;
    ref->mItem->mAmount = amount;
    return makeInstance("ItemStack", ref);
}

JSValue ScriptApi::makeItem(const ItemStack &item) {
    if (item.isAir() || item.mDefinition == nullptr)
        return JS_UNDEFINED;
    return makeItemStack(item.mDefinition->getIdentifier(), item.mCount);
}

JSValue ScriptApi::makeHeldItemStack(ServerPlayer &player, const std::string &identifier) {
    const ItemStack &stack = player.getInventory().getItemInHand();

    std::string typeId = identifier;
    int32_t count = 1;
    const Tag *dynamicProperties = nullptr;
    if (!stack.isAir() && stack.mDefinition != nullptr) {
        typeId = stack.mDefinition->getIdentifier();
        count = stack.mCount;
        if (stack.mTag.isCompound())
            dynamicProperties = stack.mTag.get("DynamicProperties");
    }

    JSValue jsItem = makeItemStack(typeId, count);
    const ScriptItemRef *ref = native<ScriptItemRef>(jsItem);
    if (dynamicProperties != nullptr && ref != nullptr)
        deserializeDynamicProperties(*dynamicProperties, ref->mItem->mDynamicProperties);
    return jsItem;
}

bool ScriptApi::giveItem(ServerPlayer &player, const std::string &typeId, int32_t amount) {
    Item item;
    if (!StringToItemParser::getInstance().parse(typeId, item))
        return false;

    std::shared_ptr<ItemDefinition> definition = mHost.getItemDefinitions().getDefinition(item.getIdentifier());
    if (definition == nullptr)
        return false;

    ItemStack stack;
    stack.mDefinition = definition;
    stack.mBlockDefinition = mHost.getBlockDefinitions().getDefinition(item.getIdentifier());
    stack.mCount = amount < 1 ? 1 : amount;

    std::vector<int> touchedSlots;
    const int remaining = player.getInventory().addItemPartial(stack, touchedSlots);
    for (const int slot: touchedSlots)
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);

    return remaining < stack.mCount;
}

bool ScriptApi::setBlockType(Level &level, int32_t x, int32_t y, int32_t z, const std::string &typeId) {
    std::string identifier = typeId;
    if (identifier.find(':') == std::string::npos)
        identifier = "minecraft:" + identifier;

    level.setBlockState(x, y, z, BlockState(identifier));

    const BlockState state = level.getBlockState(x, y, z);
    UpdateBlockPacket update;
    update.mBlockPosition = Vector3i(x, y, z);
    update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = 0;
    BlockActionHandler::broadcastToViewers(mHost, level, Vector3f((float) x + 0.5f, (float) y + 0.5f, (float) z + 0.5f),
                                           update);
    return true;
}

int32_t ScriptApi::runCommandAsPlayer(ServerPlayer &player, const std::string &commandLine) {
    CommandOriginData origin;
    origin.mOrigin = CommandOriginType::Scripting;
    origin.mPlayerId = (int64_t) player.getRuntimeId();

    PlayerCommandOrigin sender(mHost, player, origin);
    std::string line = commandLine;
    if (!line.empty() && line[0] == '/')
        line.erase(0, 1);

    return mHost.getCommands().dispatch(sender, line) ? 1 : 0;
}

int32_t ScriptApi::runCommandAsConsole(const std::string &commandLine) {
    ServerCommandOrigin sender(&mHost);
    std::string line = commandLine;
    if (!line.empty() && line[0] == '/')
        line.erase(0, 1);

    return mHost.getCommands().dispatch(sender, line) ? 1 : 0;
}

void ScriptApi::addSubscriber(const std::string &key, JSValue callback) {
    mSubscribers[key].push_back(callback);
}

void ScriptApi::removeSubscriber(const std::string &key, JSValueConst callback) {
    auto it = mSubscribers.find(key);
    if (it == mSubscribers.end())
        return;

    std::vector<JSValue> &list = it->second;
    for (auto entry = list.begin(); entry != list.end(); ++entry) {
        if (JS_VALUE_GET_PTR(*entry) == JS_VALUE_GET_PTR(callback)) {
            if (key == SCRIPT_EVENT_KEY)
                mScriptEventNamespaces.erase(JS_VALUE_GET_PTR(callback));
            JS_FreeValue(mContext, *entry);
            list.erase(entry);
            return;
        }
    }
}

bool ScriptApi::hasSubscribers(const std::string &key) const {
    auto it = mSubscribers.find(key);
    return it != mSubscribers.end() && !it->second.empty();
}

void ScriptApi::filterScriptEvents(JSValueConst callback, std::vector<std::string> namespaces) {
    mScriptEventNamespaces[JS_VALUE_GET_PTR(callback)] = std::move(namespaces);
}

bool ScriptApi::_acceptsScriptEvent(JSValueConst callback, const std::string &eventNamespace) const {
    const auto filter = mScriptEventNamespaces.find(JS_VALUE_GET_PTR(callback));
    if (filter == mScriptEventNamespaces.end())
        return true;
    return std::find(filter->second.begin(), filter->second.end(), eventNamespace) != filter->second.end();
}

bool ScriptApi::emit(const std::string &key, JSValue eventObject) {
    auto it = mSubscribers.find(key);
    if (it == mSubscribers.end() || it->second.empty()) {
        JS_FreeValue(mContext, eventObject);
        return false;
    }

    // before events and shutdown run their closures restricted, startup runs them early
    ScriptPrivilege privilege = ScriptPrivilege::Normal;
    if (key == "system.before.startup")
        privilege = ScriptPrivilege::Early;
    else if (key.compare(0, 7, "before.") == 0 || key.compare(0, 14, "system.before.") == 0)
        privilege = ScriptPrivilege::Restricted;
    // 1.x registers custom components from this before event
    if (key == "before.worldInitialize")
        privilege = ScriptPrivilege::Normal;

    const ScriptPrivilege previous = mPrivilege;
    mPrivilege = privilege;

    std::string eventNamespace;
    const bool filtered = key == SCRIPT_EVENT_KEY;
    if (filtered) {
        JSValue id = JS_GetPropertyStr(mContext, eventObject, "id");
        const std::string text = toString(mContext, id);
        JS_FreeValue(mContext, id);
        eventNamespace = text.substr(0, text.find(':'));
    }

    const std::vector<JSValue> snapshot = it->second;
    for (JSValue callback: snapshot) {
        if (filtered && !_acceptsScriptEvent(callback, eventNamespace))
            continue;

        JSValue result = JS_Call(mContext, callback, JS_UNDEFINED, 1, &eventObject);
        if (JS_IsException(result))
            logException(mContext, mPackName, "event handler (" + key + ")");
        JS_FreeValue(mContext, result);
    }

    mPrivilege = previous;

    JSValue cancelValue = JS_GetPropertyStr(mContext, eventObject, "cancel");
    const bool cancelled = JS_ToBool(mContext, cancelValue) == 1;
    JS_FreeValue(mContext, cancelValue);
    JS_FreeValue(mContext, eventObject);
    return cancelled;
}

bool ScriptApi::registerCustomComponent(bool blocks, const std::string &name, JSValueConst handlers) {
    if (!mRegistrationOpen)
        return false;

    std::unordered_map<std::string, JSValue> &registry = blocks ? mBlockComponents : mItemComponents;
    const auto existing = registry.find(name);
    if (existing != registry.end())
        JS_FreeValue(mContext, existing->second);
    registry[name] = JS_DupValue(mContext, handlers);
    return true;
}

void ScriptApi::_fireComponents(std::unordered_map<std::string, JSValue> &handlers,
                                const std::vector<std::string> *components, const char *hook, JSValue event) {
    if (components == nullptr || components->empty()) {
        JS_FreeValue(mContext, event);
        return;
    }

    for (const std::string &id: *components) {
        const auto handler = handlers.find(id);
        if (handler == handlers.end())
            continue;

        JSValue function = JS_GetPropertyStr(mContext, handler->second, hook);
        if (JS_IsFunction(mContext, function)) {
            JSValue result = JS_Call(mContext, function, handler->second, 1, &event);
            if (JS_IsException(result))
                logException(mContext, mPackName, "custom component " + id + "." + hook);
            JS_FreeValue(mContext, result);
        }
        JS_FreeValue(mContext, function);
    }
    JS_FreeValue(mContext, event);
}

void ScriptApi::fireBlockComponents(const std::string &blockId, const char *hook, JSValue event) {
    const std::vector<std::string> *components = nullptr;
    for (const CustomBlockDefinition &definition: CustomContentRegistry::getInstance().getBlocks()) {
        if (definition.mIdentifier == blockId) {
            components = &definition.mCustomComponents;
            break;
        }
    }
    _fireComponents(mBlockComponents, components, hook, event);
}

void ScriptApi::fireHeldItemComponents(ServerPlayer &player, const char *hook, JSValue event) {
    const ItemStack &held = player.getInventory().getItemInHand();
    const std::vector<std::string> *components = nullptr;
    if (!held.isAir() && held.mDefinition != nullptr) {
        const CustomItemDefinition *definition = CustomContentRegistry::getInstance().findItem(
                held.mDefinition->getIdentifier());
        if (definition != nullptr)
            components = &definition->mCustomComponents;
    }
    _fireComponents(mItemComponents, components, hook, event);
}

void ScriptApi::startup() {
    mRegistrationOpen = true;

    if (isStable1()) {
        if (hasSubscribers("before.worldInitialize")) {
            JSValue event = makeEvent("WorldBeforeEvents.worldInitialize");
            if (mSurface.has("WorldInitializeBeforeEvent.blockComponentRegistry"))
                JS_SetPropertyStr(mContext, event, "blockComponentRegistry", JS_DupValue(mContext, mBlockRegistry));
            if (mSurface.has("WorldInitializeBeforeEvent.itemComponentRegistry"))
                JS_SetPropertyStr(mContext, event, "itemComponentRegistry", JS_DupValue(mContext, mItemRegistry));
            emit("before.worldInitialize", event);
        }
        mRegistrationOpen = false;
        emit("after.worldInitialize", makeEvent("WorldAfterEvents.worldInitialize"));
        return;
    }

    // ScriptModuleStartup: registries are live only while startup runs, then early execution ends
    if (hasSubscribers("system.before.startup")) {
        JSValue event = makeEvent("SystemBeforeEvents.startup");
        JS_SetPropertyStr(mContext, event, "blockComponentRegistry", JS_DupValue(mContext, mBlockRegistry));
        JS_SetPropertyStr(mContext, event, "itemComponentRegistry", JS_DupValue(mContext, mItemRegistry));
        emit("system.before.startup", event);
    }
    mRegistrationOpen = false;
    mPrivilege = ScriptPrivilege::Normal;

    emit("after.worldLoad", makeEvent("WorldAfterEvents.worldLoad"));
}

void ScriptApi::shutdown() {
    emit("system.before.shutdown", makeEvent("SystemBeforeEvents.shutdown"));
}

JSValue ScriptApi::_makeSignalHolder(const char *className, const char *signalOwner, const char *keyPrefix) {
    JSValue holder = makeInstance(className, new ScriptNative());
    const std::string prefix = std::string(signalOwner) + ".";

    for (const ScriptSignalEntry *signal: ScriptSurface::signalsOf(signalOwner)) {
        const std::string key = signal->mProperty;
        const char *property = signal->mProperty + prefix.size();
        if (!mSurface.has(key))
            continue;

        if (JS_IsUndefined(prototypeOf(signal->mSignalClass))) {
            JSValue signalPrototype = defineClass(signal->mSignalClass);
            signalMethod(signalPrototype, signal->mSignalClass, "subscribe", signalSubscribe);
            signalMethod(signalPrototype, signal->mSignalClass, "unsubscribe", signalUnsubscribe);
        }
        if (signal->mEventClass[0] != '\0')
            defineClass(signal->mEventClass);

        ScriptSignalRef *ref = new ScriptSignalRef();
        ref->mKey = std::string(keyPrefix) + property;
        JS_DefinePropertyValueStr(mContext, holder, property, makeInstance(signal->mSignalClass, ref),
                                  JS_PROP_ENUMERABLE);
    }
    return holder;
}

void ScriptApi::_bindEvents() {
    mAfterEvents = _makeSignalHolder("WorldAfterEvents", "WorldAfterEvents", "after.");
    mBeforeEvents = _makeSignalHolder("WorldBeforeEvents", "WorldBeforeEvents", "before.");
    if (mSurface.has("System.afterEvents"))
        mSystemAfterEvents = _makeSignalHolder("SystemAfterEvents", "SystemAfterEvents", "system.after.");
    if (mSurface.has("System.beforeEvents"))
        mSystemBeforeEvents = _makeSignalHolder("SystemBeforeEvents", "SystemBeforeEvents", "system.before.");
}

void ScriptApi::_bindRegistries() {
    for (const char *name: {"BlockComponentRegistry", "ItemComponentRegistry"}) {
        if (!mSurface.has(name))
            continue;

        JSValue prototype = defineClass(name);
        method(prototype, name, "registerCustomComponent", registryRegister, 2);

        ScriptRegistryRef *ref = new ScriptRegistryRef();
        ref->mBlocks = std::strcmp(name, "BlockComponentRegistry") == 0;
        (ref->mBlocks ? mBlockRegistry : mItemRegistry) = makeInstance(name, ref);
    }
    if (mSurface.has("StartupEvent"))
        defineClass("StartupEvent");
    if (mSurface.has("ShutdownEvent"))
        defineClass("ShutdownEvent");
}

void ScriptApi::_installBootstraps() {
    const std::vector<std::string> commonErrors = mCommonSurface.errorClasses();
    const std::vector<std::string> serverErrors = mSurface.errorClasses();
    JSValue names = JS_NewArray(mContext);
    uint32_t count = 0;
    for (const std::vector<std::string> *list: {&commonErrors, &serverErrors}) {
        for (const std::string &error: *list)
            JS_SetPropertyUint32(mContext, names, count++, string(mContext, error));
    }

    mErrors = runBootstrap(mContext, mPackName, ScriptBootstraps::errors(), "<errors>", names);
    JSValue classes = JS_GetPropertyStr(mContext, mErrors, "classes");
    for (const std::string &error: commonErrors)
        mCommonExports.emplace_back(error, JS_GetPropertyStr(mContext, classes, error.c_str()));
    for (const std::string &error: serverErrors)
        mServerExports.emplace_back(error, JS_GetPropertyStr(mContext, classes, error.c_str()));
    JS_FreeValue(mContext, classes);

    JSValue data = JS_NewObject(mContext);
    addFunction(mContext, data, "blockNames", paletteBlockNames, 0);
    addFunction(mContext, data, "permutations", palettePermutations, 1);
    addFunction(mContext, data, "itemTypeNames", itemTypeNames, 0);
    JSValue dataExports = runBootstrap(mContext, mPackName, ScriptBootstraps::data(), "<data>", data);
    for (const char *name:
         {"BlockPermutation", "BlockType", "BlockTypes", "BlockStateType", "BlockStates", "ItemType", "ItemTypes"}) {
        if (mSurface.has(name))
            mServerExports.emplace_back(name, JS_GetPropertyStr(mContext, dataExports, name));
    }
    JSValue installJobs = JS_GetPropertyStr(mContext, dataExports, "installJobs");
    if (JS_IsFunction(mContext, installJobs) && mSurface.has("System.runJob")) {
        JSValue result = JS_Call(mContext, installJobs, JS_UNDEFINED, 1, &mSystem);
        JS_FreeValue(mContext, result);
    }
    JS_FreeValue(mContext, installJobs);
    JS_FreeValue(mContext, dataExports);

    JSValue world = JS_NewObject(mContext);
    addFunction(mContext, world, "setScore", scoreboardSetScore, 3);
    addFunction(mContext, world, "removeScore", scoreboardRemoveScore, 2);
    addFunction(mContext, world, "display", scoreboardDisplay, 3);
    addFunction(mContext, world, "clearDisplay", scoreboardClearDisplay, 1);
    addFunction(mContext, world, "removeObjective", scoreboardRemoveObjective, 1);
    JSValue worldExports = runBootstrap(mContext, mPackName, ScriptBootstraps::world(), "<world>", world);
    for (const char *name: {"Scoreboard", "ScoreboardObjective", "TickingAreaManager"}) {
        if (mSurface.has(name))
            mServerExports.emplace_back(name, JS_GetPropertyStr(mContext, worldExports, name));
    }
    mScoreboard = JS_GetPropertyStr(mContext, worldExports, "scoreboard");
    mTickingAreas = JS_GetPropertyStr(mContext, worldExports, "tickingAreaManager");
    JS_FreeValue(mContext, worldExports);

    if (mModules.find("@minecraft/server-ui") == nullptr)
        return;

    JSValue ui = JS_NewObject(mContext);
    addFunction(mContext, ui, "showForm", uiShowForm, 2);
    JSValue uiExports = runBootstrap(mContext, mPackName, ScriptBootstraps::ui(), "<ui>", ui);
    for (const char *name: {"ActionFormData", "MessageFormData", "ModalFormData"}) {
        if (mUiSurface.has(name))
            mUiExports.emplace_back(name, JS_GetPropertyStr(mContext, uiExports, name));
    }
    JS_FreeValue(mContext, uiExports);
}

void ScriptApi::_defineEnums() {
    for (const char *name: SERVER_ENUMS) {
        if (!mSurface.has(name))
            continue;

        JSValue object = JS_NewObject(mContext);
        for (const auto &value: mSurface.enumValues(name)) {
            JSValue member = isNumber(value.second) ? JS_NewInt32(mContext, std::atoi(value.second.c_str()))
                                                    : string(mContext, value.second);
            JS_DefinePropertyValueStr(mContext, object, value.first.c_str(), member, JS_PROP_ENUMERABLE);
        }
        mServerExports.emplace_back(name, object);
    }

    if (mCommonSurface.has("InvalidArgumentErrorType")) {
        JSValue object = JS_NewObject(mContext);
        for (const auto &value: mCommonSurface.enumValues("InvalidArgumentErrorType"))
            JS_DefinePropertyValueStr(mContext, object, value.first.c_str(), string(mContext, value.second),
                                      JS_PROP_ENUMERABLE);
        mCommonExports.emplace_back("InvalidArgumentErrorType", object);
    }

    for (const auto &component: COMPONENT_IDS) {
        JSValue constructor = constructorOf(component[0]);
        if (!JS_IsUndefined(constructor) && mSurface.has(std::string(component[0]) + ".static componentId"))
            JS_DefinePropertyValueStr(mContext, constructor, "componentId", string(mContext, component[1]),
                                      JS_PROP_ENUMERABLE);
    }
}

const std::vector<std::pair<std::string, JSValue>> &ScriptApi::moduleExports(const std::string &module) const {
    if (module == "@minecraft/server-ui")
        return mUiExports;
    if (module == "@minecraft/common")
        return mCommonExports;
    return mServerExports;
}

void ScriptApi::_registerModules() {
    for (const ScriptModuleVersion &module: mModules.getModules()) {
        if (!module.mImplemented)
            continue;
        const std::vector<std::pair<std::string, JSValue>> *exports = &moduleExports(module.mName);

        JSModuleDef *definition = JS_NewCModule(mContext, module.mName.c_str(), moduleInit);
        if (definition == nullptr)
            continue;
        for (const auto &entry: *exports)
            JS_AddModuleExport(mContext, definition, entry.first.c_str());
    }
}

namespace {
    int moduleInit(JSContext *ctx, JSModuleDef *module) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (api == nullptr)
            return -1;

        JSAtom atom = JS_GetModuleName(ctx, module);
        JSValue nameValue = JS_AtomToString(ctx, atom);
        const std::string name = toString(ctx, nameValue);
        JS_FreeValue(ctx, nameValue);
        JS_FreeAtom(ctx, atom);

        for (const auto &entry: api->moduleExports(name))
            JS_SetModuleExport(ctx, module, entry.first.c_str(), JS_DupValue(ctx, entry.second));
        return 0;
    }
}
