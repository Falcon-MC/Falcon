#include "Scripting/ScriptPackConfiguration.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Text/StringUtil.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
    std::unique_ptr<json::Value> readJson(const std::filesystem::path &path, bool &present) {
        present = false;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return nullptr;
        present = true;
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::unique_ptr<json::Value> value = json::parse(buffer.str());
        if (value == nullptr || !value->isObject()) {
            LOG_WARN(LogAreaID::Script, "Failed to parse %s", path.string().c_str());
            return nullptr;
        }
        return value;
    }

    template <typename T>
    std::optional<T> wholeNumber(const json::Value *value) {
        if (value == nullptr || !value->isNumber() || value->mNumber < 0.0
            || std::floor(value->mNumber) != value->mNumber)
            return std::nullopt;
        return (T) value->mNumber;
    }
}

const json::Value *ScriptPackConfig::findVariable(const std::string &name) const {
    for (const auto &entry: mVariables) {
        if (entry.first == name)
            return entry.second.get();
    }
    return nullptr;
}

const std::string *ScriptPackConfig::findSecret(const std::string &name) const {
    for (const auto &entry: mSecrets) {
        if (entry.first == name)
            return &entry.second;
    }
    return nullptr;
}

bool ScriptPackConfig::allowsModule(const std::string &module) const {
    if (!mHasPermissions)
        return true;
    // sub_147DBA5C0: the name itself or a lone "*"
    for (const std::string &allowed: mAllowedModules) {
        if (allowed == module || allowed == "*")
            return true;
    }
    return false;
}

ScriptNetModuleConfig ScriptPackConfig::netConfig() const {
    ScriptNetModuleConfig config;
    const auto it = mModulePermissions.find("@minecraft/server-net");
    if (it == mModulePermissions.end() || it->second == nullptr || !it->second->isObject())
        return config;

    const json::Value &net = *it->second;
    if (const json::Value *uris = net.get("allowed_uris")) {
        if (uris->isArray()) {
            std::vector<std::string> list;
            for (const auto &uri: uris->mArray) {
                if (uri->isString())
                    list.push_back(uri->mString);
            }
            config.mAllowedUris = std::move(list);
        }
    }
    // ScriptPackConfigurationManager::getNetModuleConfig: the old force_https key replaces force_tls
    const json::Value *forceTls = net.get("force_https");
    if (forceTls == nullptr)
        forceTls = net.get("force_tls");
    if (forceTls != nullptr && forceTls->mType == json::Value::Type::Boolean)
        config.mForceTls = forceTls->mBoolean;
    config.mMaxBodyBytes = wholeNumber<uint64_t>(net.get("max_body_bytes"));
    config.mMaxConcurrentRequests = wholeNumber<uint32_t>(net.get("max_concurrent_requests"));
    config.mMaxWebsocketConnections = wholeNumber<uint32_t>(net.get("max_websocket_connections"));
    config.mMaxWebsocketPayloadSize = wholeNumber<uint64_t>(net.get("max_websocket_payload_size"));
    if (const json::Value *headers = net.get("session_headers")) {
        if (headers->isObject()) {
            for (const std::string &key: headers->mKeys) {
                const json::Value *value = headers->get(key);
                if (value != nullptr && value->isString())
                    config.mSessionHeaders.emplace_back(key, value->mString);
            }
        }
    }
    return config;
}

void ScriptPackConfigurationManager::_loadFolder(const std::string &folder, ScriptPackConfig &out) {
    const std::filesystem::path root(folder);
    bool present = false;

    std::unique_ptr<json::Value> permissions = readJson(root / "permissions.json", present);
    out.mHasPermissions = present;
    if (permissions != nullptr) {
        if (const json::Value *modules = permissions->get("allowed_modules")) {
            for (const auto &module: modules->mArray) {
                if (module->isString())
                    out.mAllowedModules.push_back(module->mString);
            }
        }
        if (const json::Value *modulePermissions = permissions->get("module_permissions")) {
            for (const std::string &key: modulePermissions->mKeys)
                out.mModulePermissions[key] = modulePermissions->get(key)->clone();
        }
    }

    std::unique_ptr<json::Value> variables = readJson(root / "variables.json", present);
    if (variables != nullptr) {
        for (const std::string &key: variables->mKeys)
            out.mVariables.emplace_back(key, variables->get(key)->clone());
    }

    std::unique_ptr<json::Value> secrets = readJson(root / "secrets.json", present);
    if (secrets != nullptr) {
        for (const std::string &key: secrets->mKeys) {
            const json::Value *value = secrets->get(key);
            if (value != nullptr && value->isString())
                out.mSecrets.emplace_back(key, value->mString);
        }
    }
}

void ScriptPackConfigurationManager::load(const std::string &root) {
    mRoot = root;
    mDefault = ScriptPackConfig();
    mPacks.clear();

    std::error_code error;
    const std::filesystem::path folder(root);
    if (!std::filesystem::is_directory(folder, error))
        return;

    for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(folder, error)) {
        if (!entry.is_directory(error))
            continue;
        const std::string name = entry.path().filename().string();
        if (name == "default") {
            _loadFolder(entry.path().string(), mDefault);
            continue;
        }
        _loadFolder(entry.path().string(), mPacks[StringUtil::toLowerCase(name)]);
    }
}

const ScriptPackConfig &ScriptPackConfigurationManager::get(const std::string &moduleUuid) const {
    const auto it = mPacks.find(StringUtil::toLowerCase(moduleUuid));
    return it == mPacks.end() ? mDefault : it->second;
}
