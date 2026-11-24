#include "Falcon/Plugin/PluginPermissions.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Server/Commands/Command.h"
#include "Server/ServerPaths.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include "Core/Event/EventBus.h"

PluginPermissions::PluginPermissions(EventBus &bus)
        : mBus(bus), mGroupsPath(ServerPaths::file("permission-groups.json")) {
    if (!reloadGroups())
        LOG_WARN(LogAreaID::Server, "Invalid permission groups in %s", mGroupsPath.string().c_str());
    mLeaveHook = mBus.after().mPlayerLeave.subscribe(
            [this](PlayerLeaveAfterEvent &source) { mPlayers.erase(source.mPlayerName); });
}

PluginPermissions::~PluginPermissions() {
    mBus.after().mPlayerLeave.unsubscribe(mLeaveHook);
}

bool PluginPermissions::registerPermission(const std::string &node, FalconPermissionDefault defaultValue) {
    if (node.empty() || defaultValue > FALCON_PERMISSION_DEFAULT_OPERATOR)
        return false;
    return mDefaults.emplace(node, defaultValue).second;
}

std::optional<bool> PluginPermissions::permissionOverride(const ServerPlayer &player, const std::string &node) const {
    const auto group = _groupPermission(player, node);
    if (group == false)
        return false;

    const auto attachments = mPlayers.find(player.getName());
    if (attachments != mPlayers.end()) {
        const auto values = attachments->second.find(node);
        if (values != attachments->second.end() && !values->second.empty()) {
            for (const auto &entry: values->second) {
                if (!entry.second)
                    return false;
            }
            return true;
        }
    }

    return group;
}

bool PluginPermissions::hasPermission(const ServerPlayer &player, const std::string &node) const {
    if (const auto decision = permissionOverride(player, node))
        return *decision;

    const auto registered = mDefaults.find(node);
    if (registered == mDefaults.end() || registered->second == FALCON_PERMISSION_DEFAULT_OPERATOR)
        return player.isOp();
    return registered->second == FALCON_PERMISSION_DEFAULT_TRUE;
}

void PluginPermissions::setPermission(LoadedPlugin &plugin, const ServerPlayer &player, const std::string &node,
                                      bool value) {
    if (node.empty())
        return;
    mPlayers[player.getName()][node][&plugin] = value;
}

void PluginPermissions::unsetPermission(LoadedPlugin &plugin, const ServerPlayer &player, const std::string &node) {
    const auto attachments = mPlayers.find(player.getName());
    if (attachments == mPlayers.end())
        return;

    const auto values = attachments->second.find(node);
    if (values == attachments->second.end())
        return;

    values->second.erase(&plugin);
    if (values->second.empty())
        attachments->second.erase(values);
    if (attachments->second.empty())
        mPlayers.erase(attachments);
}

void PluginPermissions::clearPlugin(LoadedPlugin &plugin) {
    for (auto player = mPlayers.begin(); player != mPlayers.end();) {
        for (auto node = player->second.begin(); node != player->second.end();) {
            node->second.erase(&plugin);
            if (node->second.empty())
                node = player->second.erase(node);
            else
                ++node;
        }

        if (player->second.empty())
            player = mPlayers.erase(player);
        else
            ++player;
    }
}

namespace {
    bool validXuid(const std::string &value) {
        return !value.empty() && value.size() <= 20 && value.find_first_not_of("0123456789") == std::string::npos
               && value.front() != '0';
    }

    bool validKey(const std::string &value) {
        return !value.empty()
               && value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._:-")
                          == std::string::npos;
    }

    bool readNames(const json::Value *value, std::set<std::string> &names) {
        if (value == nullptr || !value->isArray())
            return false;
        for (const auto &name: value->mArray) {
            if (!name->isString() || !validKey(name->mString))
                return false;
            names.insert(name->mString);
        }
        return true;
    }

    void writeNames(std::ostream &out, const std::set<std::string> &names) {
        out << '[';
        const char *separator = "";
        for (const auto &name: names) {
            out << separator << '"' << name << '"';
            separator = ", ";
        }
        out << ']';
    }
}

bool PluginPermissions::Groups::valid() const {
    if (!mDefault.empty() && !mGroups.contains(mDefault))
        return false;

    std::map<std::string, size_t> remaining;
    std::map<std::string, std::vector<std::string>> children;
    std::vector<std::string> ready;
    for (const auto &[name, group]: mGroups) {
        remaining[name] = group.mParents.size();
        if (group.mParents.empty())
            ready.push_back(name);
        for (const auto &parent: group.mParents) {
            if (!mGroups.contains(parent))
                return false;
            children[parent].push_back(name);
        }
    }
    for (size_t index = 0; index < ready.size(); ++index) {
        const auto found = children.find(ready[index]);
        if (found == children.end())
            continue;
        for (const auto &child: found->second) {
            if (--remaining.at(child) == 0)
                ready.push_back(child);
        }
    }
    if (ready.size() != mGroups.size())
        return false;
    for (const auto &[xuid, names]: mPlayers) {
        if (!validXuid(xuid) || !std::all_of(names.begin(), names.end(), [&](const std::string &name) {
                return mGroups.contains(name);
            }))
            return false;
    }
    return true;
}

bool PluginPermissions::Groups::save(const std::filesystem::path &path) const {
    auto pending = path;
    pending += ".tmp";
    std::ofstream out(pending);
    out << "{\n  \"default\": \"" << mDefault << "\",\n  \"groups\": {";
    const char *separator = "\n";
    for (const auto &[name, group]: mGroups) {
        out << separator << "    \"" << name << "\": { \"inherits\": ";
        writeNames(out, group.mParents);
        out << ", \"permissions\": {";
        const char *nodeSeparator = "";
        for (const auto &[node, value]: group.mNodes) {
            out << nodeSeparator << " \"" << node << "\": " << (value ? "true" : "false");
            nodeSeparator = ",";
        }
        out << " } }";
        separator = ",\n";
    }
    out << "\n  },\n  \"players\": {";
    separator = "\n";
    for (const auto &[xuid, groups]: mPlayers) {
        out << separator << "    \"" << xuid << "\": ";
        writeNames(out, groups);
        separator = ",\n";
    }
    out << "\n  }\n}\n";
    out.close();
    if (!out)
        return false;

    std::error_code error;
    std::filesystem::rename(pending, path, error);
    return !error;
}

bool PluginPermissions::reloadGroups() {
    mGroupsLoaded = false;
    std::ifstream file(mGroupsPath);
    if (!file) {
        std::error_code error;
        if (std::filesystem::exists(mGroupsPath, error) || error)
            return false;
        mGroups = Groups{};
        mGroupsLoaded = true;
        return true;
    }
    std::ostringstream source;
    source << file.rdbuf();
    const auto root = json::parse(source.str());
    if (root == nullptr || !root->isObject())
        return false;

    Groups next;
    const auto *defaultGroup = root->get("default");
    const auto *groups = root->get("groups");
    const auto *players = root->get("players");
    if (defaultGroup == nullptr || !defaultGroup->isString() || groups == nullptr || !groups->isObject()
        || players == nullptr || !players->isObject())
        return false;
    next.mDefault = defaultGroup->mString;
    for (const auto &[name, value]: groups->mObject) {
        if (!validKey(name) || !value->isObject())
            return false;
        Group &group = next.mGroups[name];
        if (!readNames(value->get("inherits"), group.mParents))
            return false;
        const auto *nodes = value->get("permissions");
        if (nodes == nullptr || !nodes->isObject())
            return false;
        for (const auto &[node, enabled]: nodes->mObject) {
            if (!validKey(node) || enabled->mType != json::Value::Type::Boolean)
                return false;
            group.mNodes[node] = enabled->mBoolean;
        }
    }
    for (const auto &[xuid, value]: players->mObject) {
        if (!readNames(value.get(), next.mPlayers[xuid]))
            return false;
    }
    if (!next.valid())
        return false;
    mGroups = std::move(next);
    mGroupsLoaded = true;
    return true;
}

bool PluginPermissions::updateGroups(const std::vector<std::string> &arguments) {
    if (arguments.empty())
        return false;
    if (arguments == std::vector<std::string>{"reload"})
        return reloadGroups();
    if (!mGroupsLoaded || !std::all_of(arguments.begin(), arguments.end(), validKey))
        return false;

    Groups next = mGroups;
    const std::string &action = arguments[0];
    if (arguments.size() == 2 && action == "create") {
        if (!next.mGroups.try_emplace(arguments[1]).second)
            return false;
    } else if (arguments.size() == 2 && action == "delete") {
        if (next.mGroups.erase(arguments[1]) == 0)
            return false;
    } else if (arguments.size() == 2 && action == "default") {
        next.mDefault = arguments[1] == "-" ? "" : arguments[1];
    } else if (arguments.size() == 3 && (action == "assign" || action == "unassign")) {
        if (!validXuid(arguments[1]) || !next.mGroups.contains(arguments[2]))
            return false;
        auto &groups = next.mPlayers[arguments[1]];
        if (action == "assign")
            groups.insert(arguments[2]);
        else
            groups.erase(arguments[2]);
        if (groups.empty())
            next.mPlayers.erase(arguments[1]);
    } else if (arguments.size() >= 3 && next.mGroups.contains(arguments[1])) {
        Group &group = next.mGroups.at(arguments[1]);
        if (arguments.size() == 3 && action == "inherit")
            group.mParents.insert(arguments[2]);
        else if (arguments.size() == 3 && action == "uninherit")
            group.mParents.erase(arguments[2]);
        else if (arguments.size() == 4 && action == "set" && (arguments[3] == "true" || arguments[3] == "false"))
            group.mNodes[arguments[2]] = arguments[3] == "true";
        else if (arguments.size() == 3 && action == "unset")
            group.mNodes.erase(arguments[2]);
        else
            return false;
    } else {
        return false;
    }
    if (!next.valid() || !next.save(mGroupsPath))
        return false;
    mGroups = std::move(next);
    return true;
}

std::vector<std::string> PluginPermissions::groupNames() const {
    std::vector<std::string> names;
    for (const auto &entry: mGroups.mGroups)
        names.push_back(entry.first);
    return names;
}

std::optional<bool> PluginPermissions::_groupPermission(const ServerPlayer &player, const std::string &node) const {
    std::vector<std::string> pending;
    const auto assigned = mGroups.mPlayers.find(player.getXuid());
    if (assigned != mGroups.mPlayers.end())
        pending.insert(pending.end(), assigned->second.begin(), assigned->second.end());
    else if (!mGroups.mDefault.empty())
        pending.push_back(mGroups.mDefault);

    std::set<std::string> visited;
    bool allowed = false;
    while (!pending.empty()) {
        std::string name = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(name).second)
            continue;
        const Group &group = mGroups.mGroups.at(name);
        const auto permission = group.mNodes.find(node);
        if (permission != group.mNodes.end()) {
            if (!permission->second)
                return false;
            allowed = true;
        }
        pending.insert(pending.end(), group.mParents.begin(), group.mParents.end());
    }
    return allowed ? std::optional<bool>(true) : std::nullopt;
}

std::optional<bool> PluginPermissions::commandPermission(const ServerPlayer &player, const Command &command) const {
    // a group never grants server management: op status does
    if (command.getRequiredPermission() > CommandPermission::GameDirectors || command.managesServer())
        return std::nullopt;
    return _groupPermission(player, "falcon.command." + command.getName());
}
