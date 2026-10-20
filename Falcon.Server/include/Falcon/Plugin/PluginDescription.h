#pragma once

#include <cstdint>
#include <string>
#include <vector>

class PluginDescription {
public:
    static bool load(const std::string &path, PluginDescription &out, std::string &error);

    /**
     * A plugin runs on a server of the same major version whose API is at least as recent. The patch only
     * matters within the same minor version, where it marks functions appended to the end of the API table.
     */
    bool isApiSupported() const;

    std::string apiVersionText() const;

    static std::string serverApiVersionText();

    std::string mName;
    std::string mVersion;
    std::string mMain;
    std::string mRuntime;
    std::string mDescription;
    std::string mJar;
    std::string mAssembly;
    uint32_t mApiMajor = 0;
    uint32_t mApiMinor = 0;
    uint32_t mApiPatch = 0;
    std::vector<std::string> mAuthors;
    std::vector<std::string> mDepend;
    std::vector<std::string> mSoftDepend;
    std::vector<std::string> mLoadBefore;
};
