#include "Plugin/PluginDescription.h"

#include "Core/Json/Json.h"

#include <fstream>
#include <iterator>
#include <memory>

namespace {
    std::vector<std::string> readStrings(const json::Value &root, const char *key) {
        std::vector<std::string> values;
        const json::Value *list = root.get(key);
        if (list == nullptr || !list->isArray())
            return values;

        for (const std::unique_ptr<json::Value> &entry: list->mArray) {
            if (entry->isString() && !entry->mString.empty())
                values.push_back(entry->mString);
        }
        return values;
    }

    std::string readString(const json::Value &root, const char *key) {
        const json::Value *value = root.get(key);
        return value == nullptr ? std::string() : value->string();
    }

    bool isValidName(const std::string &name) {
        if (name.empty() || name.size() > 64)
            return false;

        for (const char character: name) {
            const bool allowed = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')
                                 || (character >= '0' && character <= '9') || character == '_' || character == '-';
            if (!allowed)
                return false;
        }
        return true;
    }

    bool parseApiVersion(const std::string &value, uint32_t &major, uint32_t &minor) {
        const size_t dot = value.find('.');
        const std::string majorText = dot == std::string::npos ? value : value.substr(0, dot);
        const std::string minorText = dot == std::string::npos ? "0" : value.substr(dot + 1);
        if (majorText.empty() || minorText.empty())
            return false;

        for (const char character: majorText + minorText) {
            if (character < '0' || character > '9')
                return false;
        }

        major = (uint32_t) std::stoul(majorText);
        minor = (uint32_t) std::stoul(minorText);
        return true;
    }
}

bool PluginDescription::load(const std::string &path, PluginDescription &out, std::string &error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }

    const std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const std::unique_ptr<json::Value> root = json::parse(source);
    if (root == nullptr || !root->isObject()) {
        error = "plugin.json is not a valid JSON object";
        return false;
    }

    out.mName = readString(*root, "name");
    out.mVersion = readString(*root, "version");
    out.mMain = readString(*root, "main");
    out.mRuntime = readString(*root, "runtime");
    out.mDescription = readString(*root, "description");
    out.mAuthors = readStrings(*root, "authors");
    out.mDepend = readStrings(*root, "depend");
    out.mSoftDepend = readStrings(*root, "softdepend");
    out.mLoadBefore = readStrings(*root, "loadbefore");

    if (out.mRuntime.empty())
        out.mRuntime = "native";

    if (!isValidName(out.mName)) {
        error = "invalid or missing name";
        return false;
    }
    if (out.mVersion.empty() || out.mMain.empty()) {
        error = "missing version or main";
        return false;
    }
    const bool native = out.mRuntime == "native";
    if (native && !parseApiVersion(readString(*root, "api-version"), out.mApiMajor, out.mApiMinor)) {
        error = "invalid or missing api-version";
        return false;
    }
    return true;
}
