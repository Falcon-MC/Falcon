#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/Binding/ScriptCustomCommand.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/ScriptEngine.h"
#include "Server/Commands/CommandEnums.h"
#include "Server/Commands/CommandMap.h"
#include "Server/Commands/ScriptCommandOrigin.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/VanillaActorTable.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <memory>

using namespace ScriptValues;

namespace {
    const char *const BOOLEAN_ENUM = "Boolean";
    const char *const LOCATION_TYPE = "Location";
    const char *const ENUM_TYPE = "Enum";
    const size_t MAX_PARAMETERS = 8;

    std::string namespaceOf(const std::string &name) {
        const size_t colon = name.find(':');
        return colon == std::string::npos ? std::string() : name.substr(0, colon);
    }

    // CustomCommandError carries a CustomCommandErrorReason
    JSValue throwCommandError(ScriptApi &api, const char *reason, const std::string &message) {
        JSValue fields = JS_NewObject(api.context());
        JS_SetPropertyStr(api.context(), fields, "reason", string(api.context(), reason));
        return api.throwError("CustomCommandError", message, fields);
    }

    // NamespaceNameError, as registerCustomDimension raises it
    JSValue checkNamespace(ScriptApi &api, const std::string &name) {
        const size_t colon = name.find(':');
        const char *reason = nullptr;
        if (colon == std::string::npos || colon == 0 || colon + 1 >= name.size())
            reason = "NoNamespace";
        else if (name.compare(0, colon, "minecraft") == 0)
            reason = "DisallowedNamespace";
        if (reason == nullptr)
            return JS_UNDEFINED;
        JSValue fields = JS_NewObject(api.context());
        JS_SetPropertyStr(api.context(), fields, "reason", string(api.context(), reason));
        return api.throwError("NamespaceNameError", "", fields);
    }

    bool readStringArray(JSContext *ctx, JSValueConst array, std::vector<std::string> &out) {
        if (!JS_IsArray(array))
            return false;
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, array, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (uint32_t index = 0; index < length; ++index) {
            JSValue item = JS_GetPropertyUint32(ctx, array, index);
            const bool ok = JS_IsString(item);
            if (ok)
                out.push_back(toString(ctx, item));
            JS_FreeValue(ctx, item);
            if (!ok)
                return false;
        }
        return true;
    }

    bool hasReason(ScriptApi &api, const char *reason) {
        for (const auto &value: api.surface().enumValues("CustomCommandErrorReason")) {
            if (value.first == reason)
                return true;
        }
        return false;
    }

    // CustomCommandParameter[]: false on a malformed entry
    bool readParameters(ScriptApi &api, JSValueConst object, const char *field, bool optional,
                        std::vector<ScriptCommandParameter> &out) {
        JSContext *ctx = api.context();
        JSValue list = JS_GetPropertyStr(ctx, object, field);
        bool ok = true;
        if (!JS_IsUndefined(list)) {
            ok = JS_IsArray(list);
            uint32_t length = 0;
            if (ok) {
                JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
                JS_ToUint32(ctx, &length, lengthValue);
                JS_FreeValue(ctx, lengthValue);
            }
            for (uint32_t index = 0; ok && index < length; ++index) {
                JSValue item = JS_GetPropertyUint32(ctx, list, index);
                JSValue name = JS_GetPropertyStr(ctx, item, "name");
                JSValue type = JS_GetPropertyStr(ctx, item, "type");
                JSValue enumName = JS_GetPropertyStr(ctx, item, "enumName");
                ok = JS_IsString(name) && JS_IsString(type);
                if (ok) {
                    ScriptCommandParameter parameter;
                    parameter.mName = toString(ctx, name);
                    parameter.mType = toString(ctx, type);
                    parameter.mOptional = optional;
                    bool known = false;
                    for (const auto &value: api.surface().enumValues("CustomCommandParamType"))
                        known = known || value.second == parameter.mType;
                    ok = known;
                    // enumName is beta: older versions name the enum after the parameter
                    parameter.mNamedEnum = JS_IsString(enumName) && hasReason(api, "UnexpectedEnumName");
                    parameter.mEnum = parameter.mNamedEnum ? toString(ctx, enumName) : parameter.mName;
                    out.push_back(std::move(parameter));
                }
                for (JSValue value: {item, name, type, enumName})
                    JS_FreeValue(ctx, value);
            }
        }
        JS_FreeValue(ctx, list);
        return ok;
    }

    JSValue registryRegisterCommand(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 2 || !JS_IsObject(argv[0]) || !JS_IsFunction(ctx, argv[1]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return ScriptApi::fromContext(ctx)->registerCustomCommand(argv[0], argv[1]);
    }

    JSValue registryRegisterEnum(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        std::vector<std::string> values;
        if (argc < 2 || !JS_IsString(argv[0]) || !readStringArray(ctx, argv[1], values))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return ScriptApi::fromContext(ctx)->registerCommandEnum(toString(ctx, argv[0]), std::move(values));
    }

    // a static get of BlockTypes, ItemTypes or EntityTypes; undefined for an unknown id
    JSValue staticGet(ScriptApi &api, const char *className, const std::string &id) {
        JSContext *ctx = api.context();
        for (const auto &entry: api.moduleExports("@minecraft/server")) {
            if (entry.first != className)
                continue;
            JSValue get = JS_GetPropertyStr(ctx, entry.second, "get");
            JSValue arg = string(ctx, id.find(':') == std::string::npos ? "minecraft:" + id : id);
            JSValue result = JS_Call(ctx, get, entry.second, 1, &arg);
            JS_FreeValue(ctx, arg);
            JS_FreeValue(ctx, get);
            if (JS_IsException(result)) {
                JS_FreeValue(ctx, JS_GetException(ctx));
                return JS_UNDEFINED;
            }
            return result;
        }
        return JS_UNDEFINED;
    }

    bool parseInteger(const std::string &token, int32_t &out) {
        if (token.empty())
            return false;
        char *end = nullptr;
        errno = 0;
        const long long parsed = std::strtoll(token.c_str(), &end, 10);
        if (*end != '\0' || errno == ERANGE || parsed < INT32_MIN || parsed > INT32_MAX)
            return false;
        out = (int32_t) parsed;
        return true;
    }

    bool parseFloat(const std::string &token, float &out) {
        if (token.empty())
            return false;
        char *end = nullptr;
        errno = 0;
        out = std::strtof(token.c_str(), &end);
        return *end == '\0' && errno != ERANGE && std::isfinite(out);
    }

    std::string usageOf(const std::string &name, const std::vector<ScriptCommandParameter> &parameters) {
        std::string usage = "/" + name;
        for (const ScriptCommandParameter &parameter: parameters) {
            const std::string type = parameter.mType == ENUM_TYPE ? parameter.mEnum : parameter.mType;
            usage += parameter.mOptional ? " [" : " <";
            usage += parameter.mName + ": " + type;
            usage += parameter.mOptional ? "]" : ">";
        }
        return usage;
    }

    std::vector<std::string> commandAliases(ServerNetworkHandler &host, const std::string &name) {
        const std::string alias = name.substr(name.find(':') + 1);
        if (host.getCommands().getCommand(alias) == nullptr)
            return {alias};
        LOG_WARN(LogAreaID::Script, "Custom Command alias [%s] already in use. Required to use full name [%s].",
                 alias.c_str(), name.c_str());
        return {};
    }
}

ScriptCustomCommand::ScriptCustomCommand(ScriptApi &api, const std::string &name, const std::string &description,
                                         CommandPermission permission, bool cheatsRequired,
                                         std::vector<ScriptCommandParameter> parameters,
                                         std::vector<std::string> aliases, JSValue callback)
        : Command(name, description, usageOf(name, parameters), aliases), mApi(api), mPermission(permission),
          mCheatsRequired(cheatsRequired), mParameters(std::move(parameters)), mCallback(callback) {}

ScriptCustomCommand::~ScriptCustomCommand() {
    JS_FreeValue(mApi.context(), mCallback);
}

std::vector<CommandOverloadData> ScriptCustomCommand::getOverloads() const {
    CommandOverloadData overload;
    for (const ScriptCommandParameter &parameter: mParameters) {
        // sub_14BD0DB00 orders the types, sub_14BD18040 maps each to the vanilla typeid: bool, int, float, string,
        // actor and player selectors, float position, then the block, entity and item enums
        CommandParamData data;
        if (parameter.mType == "Integer")
            data = makeTypedParameter(parameter.mName, CommandParamType::Int, parameter.mOptional);
        else if (parameter.mType == "Float")
            data = makeTypedParameter(parameter.mName, CommandParamType::Float, parameter.mOptional);
        else if (parameter.mType == "String")
            data = makeTypedParameter(parameter.mName, CommandParamType::String, parameter.mOptional);
        else if (parameter.mType == LOCATION_TYPE)
            data = makeTypedParameter(parameter.mName, CommandParamType::Position, parameter.mOptional);
        else if (parameter.mType == "EntitySelector" || parameter.mType == "PlayerSelector")
            data = makeTypedParameter(parameter.mName, CommandParamType::Target, parameter.mOptional);
        else if (parameter.mType == "Boolean")
            data = makeEnumParameter(parameter.mName, BOOLEAN_ENUM, {"true", "false"}, parameter.mOptional);
        else if (parameter.mType == "BlockType")
            data = makeEnumParameter(parameter.mName, "Block", CommandEnums::blockValues(), parameter.mOptional);
        else if (parameter.mType == "ItemType")
            data = makeEnumParameter(parameter.mName, "Item", CommandEnums::itemValues(), parameter.mOptional);
        else if (parameter.mType == "EntityType") {
            std::vector<std::string> identifiers;
            const char *const *table = VanillaActorTable::getIdentifiers();
            for (size_t index = 0; index < VanillaActorTable::getCount(); ++index)
                identifiers.push_back(table[index]);
            data = makeEnumParameter(parameter.mName, "EntityType", identifiers, parameter.mOptional);
            data.mEnumData.mIsSoft = true;
        } else {
            const std::vector<std::string> *values = mApi.host().getScriptEngine().findCommandEnum(parameter.mEnum);
            data = makeEnumParameter(parameter.mName, parameter.mEnum,
                                     values == nullptr ? std::vector<std::string>() : *values, parameter.mOptional);
        }
        overload.mParameters.push_back(std::move(data));
    }
    return {overload};
}

bool ScriptCustomCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (mCheatsRequired && !sender.canUseCommandsWithoutCheats() && !mApi.host().getProperties().getAllowCheats()) {
        sender.sendTranslation("commands.generic.disabled", {});
        return false;
    }

    // runCommand from a script: already inside a script call, its caller runs the microtasks
    if (dynamic_cast<ScriptCommandOrigin *>(&sender) != nullptr)
        return mApi.runCustomCommand(*this, sender, arguments);

    bool success = false;
    mApi.host().getScriptEngine().runGuarded([&]() { success = mApi.runCustomCommand(*this, sender, arguments); });
    return success;
}

JSValue ScriptApi::registerCustomCommand(JSValueConst definition, JSValueConst callback) {
    // the registry only lives while startup runs
    if (!mRegistrationOpen)
        return throwCommandError(*this, "RegistryInvalid",
                                 "Custom Command registration failed outside of the ModuleStartup event.");

    JSValue nameValue = JS_GetPropertyStr(mContext, definition, "name");
    JSValue descriptionValue = JS_GetPropertyStr(mContext, definition, "description");
    JSValue permissionValue = JS_GetPropertyStr(mContext, definition, "permissionLevel");
    JSValue cheatsValue = JS_GetPropertyStr(mContext, definition, "cheatsRequired");
    int32_t permission = -1;
    const bool typed = JS_IsString(nameValue) && JS_IsString(descriptionValue) && JS_IsNumber(permissionValue)
                       && JS_ToInt32(mContext, &permission, permissionValue) == 0;
    const std::string name = typed ? toString(mContext, nameValue) : std::string();
    const std::string description = typed ? toString(mContext, descriptionValue) : std::string();
    // cheats are required unless the command says otherwise
    const bool cheatsRequired = JS_IsUndefined(cheatsValue) || JS_ToBool(mContext, cheatsValue) == 1;
    for (JSValue value: {nameValue, descriptionValue, permissionValue, cheatsValue})
        JS_FreeValue(mContext, value);
    if (!typed || permission < (int32_t) CommandPermission::Any || permission > (int32_t) CommandPermission::Owner)
        return JS_ThrowTypeError(mContext, "Native type conversion failed.");

    std::vector<ScriptCommandParameter> parameters;
    if (!readParameters(*this, definition, "mandatoryParameters", false, parameters)
        || !readParameters(*this, definition, "optionalParameters", true, parameters))
        return JS_ThrowTypeError(mContext, "Native type conversion failed.");

    JSValue invalid = checkNamespace(*this, name);
    if (JS_IsException(invalid))
        return invalid;

    // sub_14BD1B5A0: commands and enums share the check and its enum wording
    const std::string space = namespaceOf(name);
    if (!mCommandNamespace.empty() && mCommandNamespace != space)
        return throwCommandError(*this, "NamespaceMismatch",
                                 "Custom Command Enum namespaces must match. Namespace '" + space
                                         + "' does not match existing namespace '" + mCommandNamespace + "'.");

    if (mHost.getCommands().getCommand(name) != nullptr)
        return throwCommandError(*this, "AlreadyRegistered", "Custom Command '" + name + "' was already registered.");

    // sub_14BD77D00: mandatory and optional together, at most 8
    if (parameters.size() > MAX_PARAMETERS)
        return throwCommandError(*this, "ParameterLimit",
                                 "Custom Command '" + name + "' has '" + std::to_string(parameters.size())
                                         + "' parameters, the limit is '" + std::to_string(MAX_PARAMETERS) + "'.");

    std::string missing;
    for (const ScriptCommandParameter &parameter: parameters) {
        if (parameter.mNamedEnum && parameter.mType != ENUM_TYPE)
            return throwCommandError(*this, "UnexpectedEnumName",
                                     "Custom Command parameter '" + parameter.mName + "' using enumName '"
                                             + parameter.mEnum + "' but is not an Enum type.");
        if (parameter.mType == ENUM_TYPE && mHost.getScriptEngine().findCommandEnum(parameter.mEnum) == nullptr)
            missing += (missing.empty() ? "" : ", ") + parameter.mEnum;
    }
    // sub_14BD77D00: names joined with ", "
    if (!missing.empty())
        return throwCommandError(*this, "EnumDependencyMissing",
                                 "Custom Command depends on one or more unknown enums [" + missing + "].");

    mCommandNamespace = space;
    mHost.getCommands().registerCommand(std::make_shared<ScriptCustomCommand>(
            *this, name, description, (CommandPermission) permission, cheatsRequired, std::move(parameters),
            commandAliases(mHost, name), JS_DupValue(mContext, callback)));
    mCustomCommands.push_back(name);
    return JS_UNDEFINED;
}

JSValue ScriptApi::registerCommandEnum(const std::string &name, std::vector<std::string> values) {
    if (!mRegistrationOpen)
        return throwCommandError(*this, "RegistryInvalid",
                                 "Custom Command Enum registration failed outside of the ModuleStartup event.");

    JSValue invalid = checkNamespace(*this, name);
    if (JS_IsException(invalid))
        return invalid;

    const std::string space = namespaceOf(name);
    if (!mCommandNamespace.empty() && mCommandNamespace != space)
        return throwCommandError(*this, "NamespaceMismatch",
                                 "Custom Command Enum namespaces must match. Namespace '" + space
                                         + "' does not match existing namespace '" + mCommandNamespace + "'.");

    if (mHost.getScriptEngine().findCommandEnum(name) != nullptr)
        return throwCommandError(*this, "AlreadyRegistered",
                                 "Custom Command Enum '" + name + "' was already registered.");

    mCommandNamespace = space;
    mCommandEnums[name] = std::move(values);
    return JS_UNDEFINED;
}

const std::vector<std::string> *ScriptApi::findCommandEnum(const std::string &name) const {
    const auto it = mCommandEnums.find(name);
    return it == mCommandEnums.end() ? nullptr : &it->second;
}

void ScriptApi::unregisterCustomCommands() {
    for (const std::string &name: mCustomCommands)
        mHost.getCommands().unregisterCommand(name);
    mCustomCommands.clear();
}

bool ScriptApi::runCustomCommand(const ScriptCustomCommand &command, CommandOrigin &sender,
                                 const std::vector<std::string> &arguments) {
    const std::vector<ScriptCommandParameter> &parameters = command.getParameters();
    std::vector<JSValue> args;
    const auto release = [&]() {
        for (JSValue value: args)
            JS_FreeValue(mContext, value);
    };
    const auto refuse = [&](const std::string &key, const std::vector<std::string> &parameters) {
        release();
        sender.sendTranslation(key, parameters);
        return false;
    };

    size_t token = 0;
    for (const ScriptCommandParameter &parameter: parameters) {
        const size_t needed = parameter.mType == LOCATION_TYPE ? 3 : 1;
        if (token + needed > arguments.size()) {
            if (!parameter.mOptional || token < arguments.size())
                return refuse("commands.generic.usage", {command.getUsage()});
            args.push_back(JS_UNDEFINED);
            continue;
        }
        const std::string &value = arguments[token];
        token += needed;

        if (parameter.mType == "Boolean") {
            if (value != "true" && value != "false")
                return refuse("commands.generic.bool.invalid", {value});
            args.push_back(JS_NewBool(mContext, value == "true"));
        } else if (parameter.mType == "Integer") {
            int32_t number = 0;
            if (!parseInteger(value, number))
                return refuse("commands.generic.num.invalid", {value});
            args.push_back(JS_NewInt32(mContext, number));
        } else if (parameter.mType == "Float") {
            float number = 0.0f;
            if (!parseFloat(value, number))
                return refuse("commands.generic.num.invalid", {value});
            args.push_back(JS_NewFloat64(mContext, number));
        } else if (parameter.mType == "String") {
            args.push_back(string(mContext, value));
        } else if (parameter.mType == LOCATION_TYPE) {
            Vector3f position;
            if (!Command::parsePosition(arguments, token - 3, sender.getPosition(), sender.getRotation(), position))
                return refuse("commands.generic.usage", {command.getUsage()});
            args.push_back(vector3(mContext, position.x, position.y, position.z));
        } else if (parameter.mType == "EntitySelector" || parameter.mType == "PlayerSelector") {
            JSValue list = JS_NewArray(mContext);
            uint32_t count = 0;
            for (ServerPlayer *player: mHost.resolveTargets(sender, value))
                JS_SetPropertyUint32(mContext, list, count++, makePlayer(*player));
            if (parameter.mType == "EntitySelector") {
                for (ServerActor *actor: mHost.resolveActorTargets(sender, value))
                    JS_SetPropertyUint32(mContext, list, count++, makeActor(*actor));
            }
            args.push_back(list);
            if (count == 0)
                return refuse("commands.generic.noTargetMatch", {});
        } else if (parameter.mType == "Enum") {
            const std::vector<std::string> *values = mHost.getScriptEngine().findCommandEnum(parameter.mEnum);
            if (values == nullptr || std::find(values->begin(), values->end(), value) == values->end())
                return refuse("commands.generic.usage", {command.getUsage()});
            args.push_back(string(mContext, value));
        } else {
            const char *registry = parameter.mType == "BlockType"  ? "BlockTypes"
                                   : parameter.mType == "ItemType" ? "ItemTypes"
                                                                   : "EntityTypes";
            JSValue type = staticGet(*this, registry, value);
            if (JS_IsUndefined(type) || JS_IsNull(type))
                return refuse("commands.generic.usage", {command.getUsage()});
            args.push_back(type);
        }
    }
    if (token < arguments.size())
        return refuse("commands.generic.usage", {command.getUsage()});

    // CustomCommandOrigin: an entity or player, a command block, else the server
    JSValue origin = makeInstance("CustomCommandOrigin", new ScriptNative());
    Actor *entity = sender.getEntity();
    Level *blockLevel = nullptr;
    Vector3i block;
    const char *sourceType = "Server";
    if (entity != nullptr) {
        sourceType = "Entity";
        JS_DefinePropertyValueStr(mContext, origin, "sourceEntity", makeEntity(*entity), JS_PROP_ENUMERABLE);
    } else if (sender.getSourceBlock(blockLevel, block) && blockLevel != nullptr) {
        sourceType = "Block";
        JS_DefinePropertyValueStr(mContext, origin, "sourceBlock", makeBlock(*blockLevel, block.x, block.y, block.z),
                                  JS_PROP_ENUMERABLE);
    }
    JS_DefinePropertyValueStr(mContext, origin, "sourceType", string(mContext, sourceType), JS_PROP_ENUMERABLE);

    std::vector<JSValue> callArgs;
    callArgs.push_back(origin);
    callArgs.insert(callArgs.end(), args.begin(), args.end());

    // sub_14BD03A90 leaves the context privilege (+0x1B0, sub_14A3C7470) as it is: the callback is not restricted
    JSValue result = JS_Call(mContext, command.getCallback(), JS_UNDEFINED, (int) callArgs.size(), callArgs.data());
    JS_FreeValue(mContext, origin);
    release();

    if (JS_IsException(result)) {
        // sub_14BD03A90: the thrown error's message is the command's error output
        JSValue exception = JS_GetException(mContext);
        JSValue messageValue = JS_GetPropertyStr(mContext, exception, "message");
        const std::string exceptionText = JS_IsString(messageValue) ? toString(mContext, messageValue)
                                                                    : toString(mContext, exception);
        JS_FreeValue(mContext, messageValue);
        JS_Throw(mContext, exception);
        logPendingException("custom command " + command.getName());
        sender.sendError(exceptionText);
        return false;
    }

    // CustomCommandResult: undefined is a silent success
    bool success = true;
    if (JS_IsObject(result)) {
        JSValue status = JS_GetPropertyStr(mContext, result, "status");
        JSValue message = JS_GetPropertyStr(mContext, result, "message");
        int32_t code = 0;
        JS_ToInt32(mContext, &code, status);
        success = code == 0;
        if (JS_IsString(message)) {
            // sub_14BD03A90: a Failure message goes out as error output (sub_145576240 type 1)
            const std::string text = toString(mContext, message);
            if (success)
                sender.sendMessage(text);
            else
                sender.sendError(text);
        }
        JS_FreeValue(mContext, status);
        JS_FreeValue(mContext, message);
    }
    JS_FreeValue(mContext, result);
    return success;
}

JSValue ScriptApi::runCommand(Level &level, Actor *entity, const std::string &commandLine) {
    ScriptCommandOrigin sender(level, entity);
    std::string line = commandLine;
    if (!line.empty() && line[0] == '/')
        line.erase(0, 1);

    const bool success = mHost.getCommands().dispatch(sender, line);
    // syntax, permission and unknown command errors throw; a command that ran on nothing counts zero
    const std::string &key = sender.getLastKey();
    if (!success && key.compare(0, 17, "commands.generic.") == 0 && key != "commands.generic.noTargetMatch")
        return throwError("CommandError", sender.getLastMessage());

    // commands report no count: each output names one target, a silent success counts one
    const int32_t count = success ? std::max(1, sender.getOutputCount()) : 0;
    JSValue result = makeInstance("CommandResult", new ScriptNative());
    JS_DefinePropertyValueStr(mContext, result, "successCount", JS_NewInt32(mContext, count), JS_PROP_ENUMERABLE);
    return result;
}

void ScriptApi::_bindCommands() {
    if (!mSurface.has("CustomCommandRegistry"))
        return;

    JSValue registry = defineClass("CustomCommandRegistry");
    method(registry, "CustomCommandRegistry", "registerCommand", registryRegisterCommand, 2);
    method(registry, "CustomCommandRegistry", "registerEnum", registryRegisterEnum, 2);
    mCommandRegistry = makeInstance("CustomCommandRegistry", new ScriptRegistryRef());
    if (mSurface.has("CustomCommandOrigin"))
        defineClass("CustomCommandOrigin");
}
