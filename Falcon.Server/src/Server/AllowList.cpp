#include "Server/AllowList.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>
#include <sstream>

AllowList::AllowList(const std::string &path) : mPath(path) {
    reload();
}

std::string AllowList::_toLowerCase(const std::string &value) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char character) {
                       return (char) std::tolower(character);
                   });
    return lowered;
}

std::string AllowList::_escape(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char character: value) {
        if (character == '"' || character == '\\')
            escaped.push_back('\\');
        escaped.push_back(character);
    }

    return escaped;
}

void AllowList::reload() {
    mEntries.clear();

    std::ifstream file(mPath);
    if (!file.is_open()) {
        _save();
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string source = buffer.str();

    JsonParser parser(source);
    const std::unique_ptr<JsonValue> root = parser.parse();
    if (root == nullptr || root->mType != JsonValue::Type::Array) {
        LOG_WARN(LogAreaID::Server, "Failed to parse %s", mPath.c_str());
        return;
    }

    for (const std::unique_ptr<JsonValue> &value: root->mArray) {
        if (value->mType != JsonValue::Type::Object)
            continue;

        const JsonValue *name = value->get("name");
        if (name == nullptr || name->mType != JsonValue::Type::String || name->mString.empty())
            continue;

        AllowListEntry entry;
        entry.mName = name->mString;

        const JsonValue *xuid = value->get("xuid");
        if (xuid != nullptr)
            entry.mXuid = xuid->string();

        const JsonValue *ignoresPlayerLimit = value->get("ignoresPlayerLimit");
        entry.mIgnoresPlayerLimit = ignoresPlayerLimit != nullptr
                                    && ignoresPlayerLimit->mType == JsonValue::Type::Boolean
                                    && ignoresPlayerLimit->mBoolean;

        mEntries.push_back(entry);
    }
}

std::vector<AllowListEntry>::iterator AllowList::_findByName(const std::string &name) {
    const std::string lowered = _toLowerCase(name);
    return std::find_if(mEntries.begin(), mEntries.end(), [&lowered](const AllowListEntry &entry) {
        return _toLowerCase(entry.mName) == lowered;
    });
}

bool AllowList::isAllowed(const std::string &name, const std::string &xuid) {
    if (!xuid.empty()) {
        const auto byXuid = std::find_if(mEntries.begin(), mEntries.end(), [&xuid](const AllowListEntry &entry) {
            return entry.mXuid == xuid;
        });
        if (byXuid != mEntries.end())
            return true;
    }

    const auto byName = _findByName(name);
    if (byName == mEntries.end())
        return false;

    if (!byName->mXuid.empty() && !xuid.empty())
        return false;

    if (byName->mXuid.empty() && !xuid.empty()) {
        byName->mXuid = xuid;
        _save();
    }

    return true;
}

bool AllowList::add(const std::string &name) {
    if (_findByName(name) != mEntries.end())
        return false;

    AllowListEntry entry;
    entry.mName = name;
    mEntries.push_back(entry);
    _save();
    return true;
}

bool AllowList::remove(const std::string &name) {
    const auto found = _findByName(name);
    if (found == mEntries.end())
        return false;

    mEntries.erase(found);
    _save();
    return true;
}

std::vector<std::string> AllowList::getNames() const {
    std::vector<std::string> names;
    names.reserve(mEntries.size());

    for (const AllowListEntry &entry: mEntries)
        names.push_back(entry.mName);

    return names;
}

void AllowList::_save() const {
    std::ofstream file(mPath, std::ios::trunc);
    if (!file.is_open())
        return;

    if (mEntries.empty()) {
        file << "[]\n";
        return;
    }

    file << "[\n";
    for (size_t index = 0; index < mEntries.size(); ++index) {
        const AllowListEntry &entry = mEntries[index];
        file << "    {\n";
        file << "        \"ignoresPlayerLimit\": " << (entry.mIgnoresPlayerLimit ? "true" : "false") << ",\n";
        file << "        \"name\": \"" << _escape(entry.mName) << "\"";
        if (!entry.mXuid.empty())
            file << ",\n        \"xuid\": \"" << _escape(entry.mXuid) << "\"";
        file << "\n    }" << (index + 1 < mEntries.size() ? "," : "") << "\n";
    }
    file << "]\n";
}
