#pragma once

#include "Core/NBT/Tag.h"

#include <string>

// Stores player data in a single .dat file per player, gzipped big endian NBT.
class PlayerDataProvider {
public:
    explicit PlayerDataProvider(const std::string &directory);

    bool hasData(const std::string &name) const;

    bool loadData(const std::string &name, Tag &outData) const;

    bool saveData(const std::string &name, const Tag &data) const;

private:
    std::string _getPlayerDataPath(const std::string &name) const;

    static bool _gzipCompress(const std::string &input, std::string &output);

    static bool _gzipDecompress(const std::string &input, std::string &output);

    std::string mDirectory;
};
