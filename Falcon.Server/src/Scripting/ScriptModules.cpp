#include "Scripting/ScriptModules.h"

#include <cstdlib>

namespace {
    struct KnownModule {
        const char *mName;
        // highest published minor per stable major; -1 when the major never shipped
        int mLastMinor1;
        int mLastMinor2;
        // version string a manifest gives for the beta of this game version, nullptr without one
        const char *mBeta;
        bool mImplemented;
    };

    // stable versions published for game 1.26.60, betas from the 1.26.60 stable builds
    const KnownModule KNOWN_MODULES[] = {
            {"@minecraft/server", 19, 11, "2.12.0-beta", true},
            {"@minecraft/server-ui", 3, 3, "2.4.0-beta", true},
            {"@minecraft/common", 3, -1, nullptr, true},
            {"@minecraft/server-gametest", -1, -1, "1.0.0-beta", false},
            {"@minecraft/server-admin", -1, -1, "1.0.0-beta", false},
            {"@minecraft/server-net", -1, -1, "1.0.0-beta", false},
            {"@minecraft/debug-utilities", -1, -1, "1.0.0-beta", false},
    };

    const KnownModule *findKnown(const std::string &name) {
        for (const KnownModule &known: KNOWN_MODULES) {
            if (name == known.mName)
                return &known;
        }
        return nullptr;
    }

    bool parseNumber(const std::string &text, size_t &cursor, int &out) {
        const size_t start = cursor;
        while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9')
            cursor++;
        if (cursor == start || cursor - start > 6)
            return false;
        out = std::atoi(text.substr(start, cursor - start).c_str());
        return true;
    }

    bool matchVersion(const KnownModule &known, const std::string &text, ScriptModuleVersion &out) {
        if (known.mBeta != nullptr && (text == "beta" || text == known.mBeta)) {
            out.mTrack = ScriptApiTrack::Beta;
            out.mMinor = 0;
            return true;
        }

        size_t cursor = 0;
        int major = 0, minor = 0, patch = 0;
        if (!parseNumber(text, cursor, major) || cursor >= text.size() || text[cursor++] != '.')
            return false;
        if (!parseNumber(text, cursor, minor) || cursor >= text.size() || text[cursor++] != '.')
            return false;
        if (!parseNumber(text, cursor, patch) || cursor != text.size() || patch != 0)
            return false;

        const int lastMinor = major == 1 ? known.mLastMinor1 : (major == 2 ? known.mLastMinor2 : -1);
        if (minor < 0 || minor > lastMinor)
            return false;

        out.mTrack = major == 1 ? ScriptApiTrack::Stable1 : ScriptApiTrack::Stable2;
        out.mMinor = minor;
        return true;
    }

    std::string availableVersions(const KnownModule &known) {
        std::string list;
        for (int major = 1; major <= 2; ++major) {
            const int lastMinor = major == 1 ? known.mLastMinor1 : known.mLastMinor2;
            for (int minor = 0; minor <= lastMinor; ++minor)
                list += "\n" + std::to_string(major) + "." + std::to_string(minor) + ".0";
        }
        if (known.mBeta != nullptr)
            list += std::string("\n") + known.mBeta;
        return list;
    }
}

const ScriptModuleVersion *ScriptModuleSet::find(const std::string &name) const {
    for (const ScriptModuleVersion &version: mModules) {
        if (version.mName == name)
            return &version;
    }
    return nullptr;
}

bool resolveScriptModules(const std::string &packName, const std::string &moduleUuid,
                          const std::vector<ScriptModuleDependency> &dependencies, ScriptModuleSet &out,
                          std::string &error) {
    for (const ScriptModuleDependency &dependency: dependencies) {
        const KnownModule *known = findKnown(dependency.mName);
        // ScriptPluginManager wording: the pack's scripts never start
        if (known == nullptr) {
            error = "Plugin [" + packName + "] - module [" + moduleUuid + "] depends on unknown module ["
                    + dependency.mName + "].";
            return false;
        }

        ScriptModuleVersion version;
        version.mName = dependency.mName;
        version.mVersion = dependency.mVersion;
        version.mImplemented = known->mImplemented;
        if (!matchVersion(*known, dependency.mVersion, version)) {
            error = "Plugin [" + packName + "] - module [" + moduleUuid + "] requested invalid version ["
                    + dependency.mVersion + "] of [" + dependency.mName
                    + "].\n\nAvailable versions:" + availableVersions(*known) + "\n";
            return false;
        }

        if (out.find(version.mName) != nullptr) {
            error = "Plugin [" + packName + "] - version conflict for module [" + dependency.mName + "].";
            return false;
        }
        out.add(version);
    }
    return true;
}
