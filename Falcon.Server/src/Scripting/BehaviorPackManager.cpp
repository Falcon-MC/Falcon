#include "Scripting/BehaviorPackManager.h"

#include "Core/Archive/ZipArchive.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Pack/PackDependencies.h"
#include "Server/ServerPaths.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
    std::string readFile(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return std::string();

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string findString(const std::string &json, const std::string &key, size_t from = 0) {
        const std::string pattern = "\"" + key + "\"";
        size_t position = json.find(pattern, from);
        if (position == std::string::npos)
            return std::string();

        position = json.find(':', position + pattern.size());
        if (position == std::string::npos)
            return std::string();

        position = json.find('"', position);
        if (position == std::string::npos)
            return std::string();

        const size_t end = json.find('"', position + 1);
        if (end == std::string::npos)
            return std::string();

        return json.substr(position + 1, end - position - 1);
    }

    size_t findScriptModule(const std::string &json) {
        size_t search = 0;
        for (;;) {
            const size_t typePos = json.find("\"type\"", search);
            if (typePos == std::string::npos)
                return std::string::npos;

            const size_t valueStart = json.find('"', json.find(':', typePos) + 1);
            const size_t valueEnd = json.find('"', valueStart + 1);
            if (valueStart == std::string::npos || valueEnd == std::string::npos)
                return std::string::npos;

            const std::string type = json.substr(valueStart + 1, valueEnd - valueStart - 1);
            if (type == "script")
                return typePos;

            search = valueEnd + 1;
        }
    }

    std::string findApiVersion(const std::string &json) {
        const std::string marker = "@minecraft/server";
        const size_t position = json.find(marker);
        if (position == std::string::npos)
            return std::string();

        return findString(json, "version", position);
    }
}

bool BehaviorPackManager::_readManifest(const std::string &manifest, BehaviorPack &out) {
    out.mName = findString(manifest, "name");
    out.mUuid = findString(manifest, "uuid");

    const size_t scriptModule = findScriptModule(manifest);
    if (scriptModule != std::string::npos)
        out.mScriptEntry = findString(manifest, "entry", scriptModule);

    out.mApiVersion = findApiVersion(manifest);
    out.mDependencyUuids = collectPackDependencyUuids(manifest);
    return !out.mUuid.empty();
}

void BehaviorPackManager::_addPackFromManifest(const std::string &packDirectory) {
    const std::string manifest = readFile(std::filesystem::path(packDirectory) / "manifest.json");
    if (manifest.empty())
        return;

    BehaviorPack pack;
    pack.mPath = std::filesystem::path(packDirectory).generic_string();

    if (!_readManifest(manifest, pack)) {
        LOG_WARN(LogAreaID::Server, "Behavior pack at %s has an invalid manifest.json", packDirectory.c_str());
        return;
    }

    const std::filesystem::path base(pack.mPath);
    const bool hasContent = std::filesystem::exists(base / "items") || std::filesystem::exists(base / "blocks");
    if (!pack.hasScript() && !hasContent)
        return;

    for (const BehaviorPack &existing: mPacks) {
        if (existing.mUuid == pack.mUuid) {
            LOG_WARN(LogAreaID::Server, "Skipping duplicate behavior pack (uuid %s already loaded)",
                     pack.mUuid.c_str());
            return;
        }
    }

    mPacks.push_back(pack);
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

        LOG_WARN(LogAreaID::Server, "Skipping behavior pack %s: missing dependency pack",
                 mPacks[i].mName.c_str());
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

    for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(root, error)) {
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

        const std::string bytes = readFile(entry.path());
        if (!ZipArchive::isZip(bytes)) {
            LOG_WARN(LogAreaID::Server, "Behavior pack archive %s is not a valid zip",
                     entry.path().filename().string().c_str());
            continue;
        }

        const std::filesystem::path extracted = cacheRoot / entry.path().stem();
        if (!ZipArchive::extractToDirectory(bytes, extracted.string())) {
            LOG_WARN(LogAreaID::Server, "Failed to extract behavior pack archive %s",
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

    _scanContainer(directory, cacheRoot.string());

    const std::filesystem::path bundledCache = cacheRoot / "_bundled";
    _scanContainer(ServerPaths::file("resource_packs"), bundledCache.string());
}
