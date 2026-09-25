#pragma once

#include <cstdint>
#include <string>
#include <vector>

class PluginDescription {
public:
    static bool load(const std::string &path, PluginDescription &out, std::string &error);

    std::string mName;
    std::string mVersion;
    std::string mMain;
    std::string mRuntime;
    std::string mDescription;
    uint32_t mApiMajor = 0;
    uint32_t mApiMinor = 0;
    std::vector<std::string> mAuthors;
    std::vector<std::string> mDepend;
    std::vector<std::string> mSoftDepend;
    std::vector<std::string> mLoadBefore;
};
