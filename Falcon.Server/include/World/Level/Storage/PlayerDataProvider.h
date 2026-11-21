#pragma once

#include "Core/NBT/Tag.h"

#include <string>

// one .dat file per player: gzipped big endian NBT
class PlayerDataProvider {
public:
    explicit PlayerDataProvider(const std::string &directory);

    bool hasData(const std::string &name) const;

    bool loadData(const std::string &name, Tag &outData) const;

    bool saveData(const std::string &name, const Tag &data) const;

    bool removeData(const std::string &name) const;

    // xuid a save was written for, kept inside it
    static constexpr const char *TAG_OWNER_XUID = "FalconOwnerXuid";

    // makes the save under this name the one of this account: another account's is set aside under its
    // own xuid and this account's earlier one brought back. false when that could not be done, and
    // nothing may then be saved under the name
    bool claimData(const std::string &name, const std::string &xuid) const;

    // no path separators, no traversal; every name used as a file goes through it
    static bool isSafeName(const std::string &name);

private:
    std::string _getArchivePath(const std::string &name, const std::string &xuid) const;


    std::string _getPlayerDataPath(const std::string &name) const;

    static bool _gzipCompress(const std::string &input, std::string &output);

    static bool _gzipDecompress(const std::string &input, std::string &output);

    std::string mDirectory;
};
