#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// module_permissions."@minecraft/server-net" of permissions.json: an option left out is not enforced
struct ScriptNetModuleConfig {
    std::optional<std::vector<std::string>> mAllowedUris;
    std::optional<bool> mForceTls;
    std::optional<uint64_t> mMaxBodyBytes;
    std::optional<uint32_t> mMaxConcurrentRequests;
    std::vector<std::pair<std::string, std::string>> mSessionHeaders;
    std::optional<uint32_t> mMaxWebsocketConnections;
    std::optional<uint64_t> mMaxWebsocketPayloadSize;
};

// one folder under config/: permissions.json, variables.json and secrets.json
struct ScriptPackConfig {
    bool mHasPermissions = false;
    std::vector<std::string> mAllowedModules;
    std::unordered_map<std::string, std::unique_ptr<json::Value>> mModulePermissions;
    // names in file order
    std::vector<std::pair<std::string, std::unique_ptr<json::Value>>> mVariables;
    std::vector<std::pair<std::string, std::string>> mSecrets;

    const json::Value *findVariable(const std::string &name) const;

    const std::string *findSecret(const std::string &name) const;

    // without permissions.json every module is allowed
    bool allowsModule(const std::string &module) const;

    ScriptNetModuleConfig netConfig() const;
};

// ScriptPackConfigurationManager: config/<script module uuid>/ when that folder exists, config/default/ otherwise
class ScriptPackConfigurationManager {
public:
    // root: the config folder
    void load(const std::string &root);

    void reload() {
        load(mRoot);
    }

    const ScriptPackConfig &get(const std::string &moduleUuid) const;

private:
    static void _loadFolder(const std::string &folder, ScriptPackConfig &out);

    std::string mRoot;
    ScriptPackConfig mDefault;
    std::unordered_map<std::string, ScriptPackConfig> mPacks;
};
