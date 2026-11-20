#include "Server/Commands/InputPermissionCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>
#include <utility>

namespace {
    // the script API's InputPermissionCategory values; 3 is unused
    const std::vector<std::pair<std::string, int32_t>> &permissions() {
        static const std::vector<std::pair<std::string, int32_t>> PERMISSIONS = {
                {"camera", 1},        {"movement", 2},      {"lateral_movement", 4}, {"sneak", 5},
                {"jump", 6},          {"mount", 7},         {"dismount", 8},         {"move_forward", 9},
                {"move_backward", 10}, {"move_left", 11},   {"move_right", 12}};
        return PERMISSIONS;
    }

    const std::vector<std::string> STATES = {"enabled", "disabled"};

    const std::pair<std::string, int32_t> *findPermission(const std::string &name) {
        for (const auto &permission: permissions()) {
            if (permission.first == name)
                return &permission;
        }
        return nullptr;
    }

    // the client translates a parameter that starts with %
    std::string translated(const std::string &name) {
        return "%commands.inputpermission." + name;
    }
}

InputPermissionCommand::InputPermissionCommand(ServerNetworkHandler &handler)
        : Command("inputpermission", "commands.inputpermission.description",
                  "/inputpermission <query|set> <targets> <permission> [state]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> InputPermissionCommand::getOverloads() const {
    std::vector<std::string> names;
    for (const auto &permission: permissions())
        names.push_back(permission.first);

    CommandOverloadData query;
    query.mParameters.push_back(makeEnumParameter("option", "InputPermissionQuery", {"query"}));
    query.mParameters.push_back(makePlayerParameter("targets"));
    query.mParameters.push_back(makeEnumParameter("permission", "InputPermission", names));
    query.mParameters.push_back(makeEnumParameter("state", "InputPermissionState", STATES, true));

    CommandOverloadData set;
    set.mParameters.push_back(makeEnumParameter("option", "InputPermissionSet", {"set"}));
    set.mParameters.push_back(makePlayerParameter("targets"));
    set.mParameters.push_back(makeEnumParameter("permission", "InputPermission", names));
    set.mParameters.push_back(makeEnumParameter("state", "InputPermissionState", STATES));
    return {query, set};
}

bool InputPermissionCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 3 || (arguments[0] != "query" && arguments[0] != "set")) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    const bool setting = arguments[0] == "set";
    const std::pair<std::string, int32_t> *permission = findPermission(arguments[2]);
    if (permission == nullptr) {
        sender.sendTranslation("commands.inputpermission.set.missingpermission", {});
        return false;
    }
    const bool hasState = arguments.size() > 3;
    const bool validState = hasState && std::find(STATES.begin(), STATES.end(), arguments[3]) != STATES.end();
    if (setting && !validState) {
        sender.sendTranslation("commands.inputpermission.set.missingstate", {});
        return false;
    }
    if (!setting && hasState && !validState) {
        sender.sendTranslation("commands.inputpermission.set.invalidfilter", {arguments[3]});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[1]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    const int32_t category = permission->second;
    if (!setting) {
        int32_t enabled = 0;
        for (const ServerPlayer *target: targets)
            enabled += target->isInputLocked(category) ? 0 : 1;
        const int32_t disabled = (int32_t) targets.size() - enabled;
        if (!hasState) {
            sender.sendTranslation("commands.inputpermission.queryverbose",
                                   {translated(permission->first), std::to_string(enabled), std::to_string(disabled)});
            return true;
        }
        const bool wantEnabled = arguments[3] == "enabled";
        sender.sendTranslation("commands.inputpermission.query",
                               {translated(permission->first), std::to_string(wantEnabled ? enabled : disabled),
                                translated(arguments[3])});
        return true;
    }

    const bool enable = arguments[3] == "enabled";
    for (ServerPlayer *target: targets) {
        if (target->setInputLocked(category, !enable))
            mHandler.getScriptEngine().onPlayerInputPermissionCategoryChange(*target, category, enable);
    }
    if (targets.size() == 1)
        sender.sendTranslation("commands.inputpermission.set.outputoneplayer",
                               {translated(permission->first), translated(arguments[3]), targets.front()->getName()});
    else
        sender.sendTranslation("commands.inputpermission.set.outputmultipleplayers",
                               {translated(permission->first), translated(arguments[3]),
                                std::to_string(targets.size())});
    return true;
}
