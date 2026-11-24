#include "Scripting/BehaviorPackManager.h"

#include "Core/Archive/ZipArchive.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Core/Text/StringUtil.h"
#include "Resources/PackDependencies.h"
#include "Server/ServerPaths.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace {
    std::string readFile(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return std::string();

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // manifests may carry comments: the game reads them leniently
    std::string stripComments(const std::string &json) {
        std::string result;
        result.reserve(json.size());
        bool inString = false;
        for (size_t i = 0; i < json.size(); ++i) {
            const char character = json[i];
            if (inString) {
                result.push_back(character);
                if (character == '\\' && i + 1 < json.size())
                    result.push_back(json[++i]);
                else if (character == '"')
                    inString = false;
                continue;
            }
            if (character == '"') {
                inString = true;
                result.push_back(character);
            } else if (character == '/' && i + 1 < json.size() && json[i + 1] == '/') {
                while (i < json.size() && json[i] != '\n')
                    ++i;
                result.push_back('\n');
            } else if (character == '/' && i + 1 < json.size() && json[i + 1] == '*') {
                i += 2;
                while (i + 1 < json.size() && !(json[i] == '*' && json[i + 1] == '/'))
                    ++i;
                ++i;
            } else {
                result.push_back(character);
            }
        }
        return result;
    }

    std::string versionText(const json::Value *version) {
        if (version == nullptr)
            return std::string();
        if (version->isString())
            return version->string();
        if (!version->isArray())
            return std::string();

        std::string text;
        for (const std::unique_ptr<json::Value> &part: version->mArray) {
            if (!part->isNumber())
                return std::string();
            if (!text.empty())
                text += ".";
            text += std::to_string(part->integer(0));
        }
        return text;
    }
}

bool BehaviorPackManager::_readManifest(const std::string &manifest, BehaviorPack &out) {
    const std::unique_ptr<json::Value> root = json::parse(stripComments(manifest));
    const json::Value *header = root && root->isObject() ? root->get("header") : nullptr;
    if (!header || !header->isObject() || !header->get("uuid"))
        return false;
    out.mUuid = header->get("uuid")->string();
    if (out.mUuid.empty())
        return false;
    if (const auto *name = header->get("name"))
        out.mName = name->string();

    const json::Value *modules = root->get("modules");
    if (modules && modules->isArray()) {
        for (const auto &module: modules->mArray) {
            if (!module->isObject() || !module->get("type") || module->get("type")->string() != "script")
                continue;
            if (out.hasScript() || !module->get("uuid") || !module->get("entry"))
                return false;
            out.mScriptModuleUuid = module->get("uuid")->string();
            out.mScriptEntry = module->get("entry")->string();
            if (out.mScriptModuleUuid.empty() || out.mScriptEntry.empty())
                return false;
            out.mScriptModuleVersion = versionText(module->get("version"));
        }
    }

    const json::Value *settings = root->get("settings");
    if (settings != nullptr && settings->isArray()) {
        for (const std::unique_ptr<json::Value> &entry: settings->mArray) {
            const json::Value *name = entry->isObject() ? entry->get("name") : nullptr;
            const json::Value *value = entry->isObject() ? entry->get("default") : nullptr;
            // labels carry no name and no value
            if (name == nullptr || !name->isString() || value == nullptr)
                continue;

            PackSetting setting;
            setting.mName = name->string();
            if (value->mType == json::Value::Type::Boolean) {
                setting.mType = PackSetting::Type::Boolean;
                setting.mBoolean = value->mBoolean;
            } else if (value->isNumber()) {
                setting.mType = PackSetting::Type::Number;
                setting.mNumber = value->mNumber;
            } else if (value->isString()) {
                setting.mType = PackSetting::Type::String;
                setting.mString = value->mString;
            } else {
                continue;
            }
            out.mSettings.push_back(setting);
        }
    }

    out.mDependencyUuids = collectPackDependencyUuids(manifest);
    const json::Value *dependencies = root->get("dependencies");
    if (dependencies == nullptr || !dependencies->isArray())
        return true;

    for (const std::unique_ptr<json::Value> &dependency: dependencies->mArray) {
        if (!dependency->isObject() || dependency->get("module_name") == nullptr)
            continue;

        ScriptModuleDependency module;
        module.mName = dependency->get("module_name")->string();
        module.mVersion = versionText(dependency->get("version"));
        out.mScriptModules.push_back(module);
    }
    return true;
}

void BehaviorPackManager::_addPackFromManifest(const std::string &packDirectory) {
    const std::string manifest = readFile(std::filesystem::path(packDirectory) / "manifest.json");
    if (manifest.empty())
        return;

    BehaviorPack pack;
    pack.mPath = std::filesystem::path(packDirectory).generic_string();

    if (!_readManifest(manifest, pack)) {
        LOG_WARN(LogAreaID::Resource, "Behavior pack at %s has an invalid manifest.json", packDirectory.c_str());
        return;
    }

    const std::filesystem::path base(pack.mPath);
    const bool hasContent = std::filesystem::exists(base / "items") || std::filesystem::exists(base / "blocks")
                            || std::filesystem::exists(base / "sounds" / "sound_definitions.json");
    if (!pack.hasScript() && !hasContent)
        return;

    mPacks.push_back(std::move(pack));
}

std::vector<std::string> BehaviorPackManager::getLoadedUuids() const {
    std::vector<std::string> uuids;
    uuids.reserve(mPacks.size());
    for (const BehaviorPack &pack: mPacks)
        uuids.push_back(pack.mUuid);
    return uuids;
}

size_t BehaviorPackManager::pruneUnsatisfied(const std::vector<std::string> &availableUuids) {
    size_t removed = 0;

    for (size_t i = 0; i < mPacks.size();) {
        bool satisfied = true;
        for (const std::string &dependency: mPacks[i].mDependencyUuids) {
            if (std::find(availableUuids.begin(), availableUuids.end(), dependency) == availableUuids.end()) {
                satisfied = false;
                break;
            }
        }

        if (satisfied) {
            ++i;
            continue;
        }

        LOG_WARN(LogAreaID::Resource, "Skipping behavior pack %s: missing dependency pack", mPacks[i].mName.c_str());
        mPacks.erase(mPacks.begin() + (long) i);
        removed++;
    }

    return removed;
}

void BehaviorPackManager::_scanRoot(const std::string &root) {
    std::error_code error;
    const std::filesystem::path base(root);

    if (!std::filesystem::exists(base / "manifest.json", error)) {
        for (const std::filesystem::directory_entry &entry:
             std::filesystem::recursive_directory_iterator(base, error)) {
            if (entry.is_regular_file() && entry.path().filename() == "manifest.json")
                _addPackFromManifest(entry.path().parent_path().string());
        }
        return;
    }

    _addPackFromManifest(root);
}

void BehaviorPackManager::_scanContainer(const std::string &directory, const std::string &cacheDirectory) {
    std::error_code error;
    const std::filesystem::path root(directory);
    if (!std::filesystem::exists(root, error))
        return;

    const std::filesystem::path cacheRoot(cacheDirectory);

    // directory order differs between file systems, the pack order must not
    std::vector<std::filesystem::directory_entry> entries;
    for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(root, error))
        entries.push_back(entry);
    std::sort(entries.begin(), entries.end(),
              [](const std::filesystem::directory_entry &a, const std::filesystem::directory_entry &b) {
                  return a.path().filename() < b.path().filename();
              });

    for (const std::filesystem::directory_entry &entry: entries) {
        if (entry.path().filename() == ".cache")
            continue;

        if (entry.is_directory()) {
            _scanRoot(entry.path().string());
            continue;
        }

        if (!entry.is_regular_file())
            continue;

        const std::string extension = entry.path().extension().string();
        if (extension != ".mcpack" && extension != ".mcaddon" && extension != ".zip")
            continue;

        if (entry.file_size(error) > ZipArchive::MAX_ARCHIVE_BYTES || error) {
            LOG_WARN(LogAreaID::Resource, "Behavior pack archive %s exceeds the read budget or is unreadable",
                     entry.path().filename().string().c_str());
            continue;
        }
        const std::string bytes = readFile(entry.path());
        if (!ZipArchive::isZip(bytes)) {
            LOG_WARN(LogAreaID::Resource, "Behavior pack archive %s is not a valid zip",
                     entry.path().filename().string().c_str());
            continue;
        }

        const std::filesystem::path extracted = cacheRoot / entry.path().filename();
        if (!ZipArchive::extractToDirectory(bytes, extracted.string())) {
            LOG_WARN(LogAreaID::Resource, "Failed to extract behavior pack archive %s",
                     entry.path().filename().string().c_str());
            continue;
        }

        _scanRoot(extracted.string());
    }
}

void BehaviorPackManager::discover(const std::string &directory) {
    mPacks.clear();

    std::error_code error;
    const std::filesystem::path root(directory);
    if (!std::filesystem::exists(root, error))
        std::filesystem::create_directories(root, error);

    const std::filesystem::path cacheRoot = root / ".cache";
    std::filesystem::remove_all(cacheRoot, error);

    _scanContainer(directory, (cacheRoot / "behavior").string());

    const std::filesystem::path bundledCache = cacheRoot / "bundled";
    _scanContainer(ServerPaths::file("resource_packs"), bundledCache.string());

    // a module UUID selects server-side permissions and secrets: never let directory order choose its owner
    std::unordered_map<std::string, size_t> modules;
    for (const BehaviorPack &pack: mPacks) {
        if (pack.hasScript())
            ++modules[StringUtil::toLowerCase(pack.mScriptModuleUuid)];
    }
    std::unordered_set<std::string> packIds;
    std::erase_if(mPacks, [&](const BehaviorPack &pack) {
        if (pack.hasScript() && modules.at(StringUtil::toLowerCase(pack.mScriptModuleUuid)) > 1) {
            LOG_WARN(LogAreaID::Resource, "Skipping %s: script module UUID %s has multiple owners",
                     pack.mPath.c_str(), pack.mScriptModuleUuid.c_str());
            return true;
        }
        if (!packIds.insert(StringUtil::toLowerCase(pack.mUuid)).second) {
            LOG_WARN(LogAreaID::Resource, "Skipping duplicate behavior pack (uuid %s already loaded)",
                     pack.mUuid.c_str());
            return true;
        }
        return false;
    });
}
