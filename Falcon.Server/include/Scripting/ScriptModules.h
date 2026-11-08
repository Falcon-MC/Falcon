#pragma once

#include <string>
#include <vector>

// 1.x and 2.x route to distinct native bindings; the beta follows 2.x with its own extra surface
enum class ScriptApiTrack {
    Stable1,
    Stable2,
    Beta
};

struct ScriptModuleDependency {
    std::string mName;
    std::string mVersion;
};

struct ScriptModuleVersion {
    std::string mName;
    std::string mVersion;
    ScriptApiTrack mTrack = ScriptApiTrack::Stable2;
    int mMinor = 0;
    // a module the game accepts but this server does not implement
    bool mImplemented = true;
};

class ScriptModuleSet {
public:
    const ScriptModuleVersion *find(const std::string &name) const;

    const std::vector<ScriptModuleVersion> &getModules() const {
        return mModules;
    }

    void add(const ScriptModuleVersion &version) {
        mModules.push_back(version);
    }

private:
    std::vector<ScriptModuleVersion> mModules;
};

// false and a content log error in the game's wording when one dependency names an unknown module or version
bool resolveScriptModules(const std::string &packName, const std::string &moduleUuid,
                          const std::vector<ScriptModuleDependency> &dependencies, ScriptModuleSet &out,
                          std::string &error);
