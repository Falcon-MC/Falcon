#pragma once

#include "Core/Utility/UUID.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class ResourcePack {
public:
    Uuid mUuid;
    std::string mUuidString;
    std::string mVersion;
    std::string mName;
    std::string mContentKey;
    std::string mData;
    std::string mSha256;
    std::string mCdnUrl;
    uint64_t mSize = 0;
    std::vector<std::string> mDependencyUuids;
};

struct ResourcePackOffer {
    std::vector<ResourcePack> mPacks;
    bool mRequired = false;

    const ResourcePack *findById(const std::string &uuid) const;
};

class ResourcePackManager {
public:
    void loadFromDirectory(const std::string &directory);

    void loadBundledAddonsFrom(const std::string &directory);

    bool loadCdnConfig(const std::string &path);

    void loadFolder(const std::string &path, const std::string &label);

    const std::vector<ResourcePack> &getPacks() const {
        return mPacks;
    }

    bool replacePluginPacks(const std::string &owner, std::vector<std::pair<std::string, std::string>> archives,
                            bool required);

    std::shared_ptr<const ResourcePackOffer> getOffer(bool serverRequired);

    size_t count() const {
        return mPacks.size();
    }

    std::vector<std::string> getLoadedUuids() const;

    size_t pruneUnsatisfied(const std::vector<std::string> &availableUuids);

private:
    bool _loadPackFile(const std::string &path, const std::string &fileName);

    bool _loadPackBytes(std::string data, const std::string &label, const std::string &contentKey, size_t maxManifestSize = SIZE_MAX);

    void _loadFolder(const std::string &path, const std::string &label);

    void _loadAddon(const std::string &data);

    static bool _readWholeFile(const std::string &path, std::string &out);

    static bool _extractZipEntry(const std::string &zip, const std::string &suffix, std::string &out, size_t maxSize);

    static bool _readManifest(const std::string &manifest, std::string &uuid, std::string &version, std::string &name);

    static std::string _sha256(const std::string &data);

    std::vector<ResourcePack> mPacks;
    std::map<std::string, ResourcePackOffer> mPluginStacks;
    std::shared_ptr<const ResourcePackOffer> mCachedOffer;
};
