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

    // sets a file aside under another suffix, so the name can be saved again without losing it
    bool archiveData(const std::string &name, const std::string &suffix) const;

    // no path separators, no traversal; every name used as a file goes through it
    static bool isSafeName(const std::string &name);

private:

    std::string _getPlayerDataPath(const std::string &name) const;

    static bool _gzipCompress(const std::string &input, std::string &output);

    static bool _gzipDecompress(const std::string &input, std::string &output);

    std::string mDirectory;
};
