#include "Server/PropertiesSettings.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace {
    const int UNBOUNDED = 0x7FFFFFFF;

    bool isInteger(const std::string &value) {
        if (value.empty())
            return false;

        size_t index = value[0] == '-' ? 1 : 0;
        if (index >= value.size())
            return false;

        for (; index < value.size(); ++index) {
            if (std::isdigit((unsigned char) value[index]) == 0)
                return false;
        }

        return true;
    }

    bool isFloating(const std::string &value) {
        if (value.empty())
            return false;

        char *end = nullptr;
        std::strtod(value.c_str(), &end);
        return end != value.c_str() && *end == '\0';
    }

    bool isListed(const std::string &value, const char *values) {
        if (values == nullptr)
            return true;

        std::stringstream stream(values);
        std::string candidate;

        while (std::getline(stream, candidate, ',')) {
            if (candidate == value)
                return true;
        }

        return false;
    }
}

const std::vector<PropertyDefinition> &PropertiesSettings::getDefinitions() {
    static const std::vector<PropertyDefinition> definitions = {
            {"server-name", "Falcon Server", PropertyKind::String, 0, 0, nullptr},
            {"gamemode", "survival", PropertyKind::Enum, 0, 0, "survival,creative,adventure,spectator"},
            {"force-gamemode", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"difficulty", "easy", PropertyKind::Enum, 0, 0, "peaceful,easy,normal,hard"},
            {"allow-cheats", "true", PropertyKind::Bool, 0, 0, nullptr},
            {"max-players", "10", PropertyKind::Int, 1, UNBOUNDED, nullptr},
            {"online-mode", "true", PropertyKind::Bool, 0, 0, nullptr},
            {"allow-list", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"server-port", "19132", PropertyKind::Int, 1, 65535, nullptr},
            {"server-portv6", "19133", PropertyKind::Int, 1, 65535, nullptr},
            {"enable-lan-visibility", "true", PropertyKind::Bool, 0, 0, nullptr},
            {"view-distance", "32", PropertyKind::Int, 4, 96, nullptr},
            {"tick-distance", "4", PropertyKind::Int, 4, 12, nullptr},
            {"player-idle-timeout", "30", PropertyKind::Int, 0, UNBOUNDED, nullptr},
            {"max-threads", "8", PropertyKind::Int, 0, 256, nullptr},
            {"autosave-interval", "6000", PropertyKind::Int, 0, UNBOUNDED, nullptr},
            {"level-name", "Bedrock level", PropertyKind::String, 0, 0, nullptr},
            {"level-seed", "", PropertyKind::String, 0, 0, nullptr},
            {"default-player-permission-level", "member", PropertyKind::Enum, 0, 0, "visitor,member,operator"},
            {"texturepack-required", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"content-log-file-enabled", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"content-log-console-output-enabled", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"content-log-level", "info", PropertyKind::Enum, 0, 0, "error,warning,info,verbose"},
            {"compression-threshold", "1", PropertyKind::Int, 0, 65535, nullptr},
            {"compression-algorithm", "zlib", PropertyKind::Enum, 0, 0, "zlib,snappy"},
            {"chat-restriction", "None", PropertyKind::Enum, 0, 0, "None,Dropped,Disabled"},
            {"disable-player-interaction", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"client-side-chunk-generation-enabled", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"block-network-ids-are-hashes", "true", PropertyKind::Bool, 0, 0, nullptr},
            {"disable-custom-skins", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"skin-change-cooldown", "30", PropertyKind::Int, 0, UNBOUNDED, nullptr},
            {"spawn-protection", "16", PropertyKind::Int, 0, UNBOUNDED, nullptr},
            {"server-authoritative-movement-strict", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"server-authoritative-dismount-strict", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"server-authoritative-entity-interactions-strict", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"server-authoritative-block-breaking-pick-range-scalar", "1.5", PropertyKind::Float, 0, 0, nullptr},
            {"server-build-radius-ratio", "Disabled", PropertyKind::String, 0, 0, nullptr},
            {"player-position-acceptance-threshold", "0.5", PropertyKind::Float, 0, 0, nullptr},
            {"player-movement-action-direction-threshold", "0.85", PropertyKind::Float, 0, 0, nullptr},
            {"allow-inbound-script-debugging", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"allow-outbound-script-debugging", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"script-debugger-auto-attach", "disabled", PropertyKind::String, 0, 0, nullptr},
            {"disable-persona", "false", PropertyKind::Bool, 0, 0, nullptr},
            {"fluid-budget-ms", "20", PropertyKind::Int, -1, UNBOUNDED, nullptr},
            {"auto-compaction-interval", "360", PropertyKind::Int, 0, UNBOUNDED, nullptr},
            {"transport", "raknet", PropertyKind::Enum, 0, 0, "raknet,nethernet"}
    };

    return definitions;
}

const PropertyDefinition *PropertiesSettings::findDefinition(const std::string &key) {
    for (const PropertyDefinition &definition: getDefinitions()) {
        if (key == definition.mKey)
            return &definition;
    }

    return nullptr;
}

std::string PropertiesSettings::getDefaultContents() {
    std::string contents;

    for (const PropertyDefinition &definition: getDefinitions()) {
        contents += definition.mKey;
        contents += "=";
        contents += definition.mDefault;
        contents += "\n";
    }

    return contents;
}

PropertiesSettings::PropertiesSettings() : mLoaded(false) {}

PropertiesSettings::PropertiesSettings(const std::string &path) : mLoaded(false) {
    load(path);
}

std::string PropertiesSettings::_trim(const std::string &value) {
    size_t start = 0;
    size_t end = value.size();

    while (start < end && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r'))
        start++;

    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r'))
        end--;

    return value.substr(start, end - start);
}

void PropertiesSettings::_writeDefault(const std::string &path) {
    std::ofstream file(path);
    if (!file.is_open())
        return;

    file << getDefaultContents();
}

bool PropertiesSettings::_isValidValue(const PropertyDefinition &definition, const std::string &value) {
    switch (definition.mKind) {
        case PropertyKind::String:
            return true;
        case PropertyKind::Bool:
            return value == "true" || value == "false" || value == "1" || value == "0";
        case PropertyKind::Int: {
            if (!isInteger(value))
                return false;

            const long parsed = std::strtol(value.c_str(), nullptr, 10);
            return parsed >= definition.mMinimum && parsed <= definition.mMaximum;
        }
        case PropertyKind::Float:
            return isFloating(value);
        case PropertyKind::Enum:
            return isListed(value, definition.mValues);
    }

    return true;
}

void PropertiesSettings::_validate() {
    mInvalid.clear();

    for (const PropertyDefinition &definition: getDefinitions()) {
        const auto it = mProperties.find(definition.mKey);
        if (it == mProperties.end())
            continue;

        if (it->second.empty() && definition.mKind != PropertyKind::String) {
            it->second = definition.mDefault;
            continue;
        }

        if (_isValidValue(definition, it->second))
            continue;

        mInvalid.push_back(std::string(definition.mKey) + "=" + it->second);
        it->second = definition.mDefault;
    }
}

TransportLayer PropertiesSettings::getTransportLayer() const {
    const std::string value = getString("transport");

    if (value.empty() || value == "raknet")
        return TransportLayer::RakNet;
    if (value == "nethernet")
        return TransportLayer::NetherNet;

    return TransportLayer::Unknown;
}

bool PropertiesSettings::load(const std::string &path) {
    mPath = path;
    mProperties.clear();
    mLoaded = false;

    std::ifstream file(path);
    if (!file.is_open()) {
        _writeDefault(path);
        file.open(path);
    }

    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = _trim(line);
        if (trimmed.empty() || trimmed[0] == '#')
            continue;

        const size_t separator = trimmed.find('=');
        if (separator == std::string::npos)
            continue;

        const std::string key = _trim(trimmed.substr(0, separator));
        if (key.empty())
            continue;

        mProperties[key] = _trim(trimmed.substr(separator + 1));
    }

    _validate();

    mLoaded = true;
    return true;
}

bool PropertiesSettings::setProperty(const std::string &key, const std::string &value) {
    mProperties[key] = value;

    std::vector<std::string> lines;
    bool replaced = false;

    std::ifstream input(mPath);
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = _trim(line);
        const size_t separator = trimmed.find('=');
        if (!replaced && !trimmed.empty() && trimmed[0] != '#' && separator != std::string::npos
            && _trim(trimmed.substr(0, separator)) == key) {
            lines.push_back(key + "=" + value);
            replaced = true;
            continue;
        }

        lines.push_back(line);
    }
    input.close();

    if (!replaced)
        lines.push_back(key + "=" + value);

    std::ofstream output(mPath, std::ios::trunc);
    if (!output.is_open())
        return false;

    for (const std::string &entry: lines)
        output << entry << "\n";

    return true;
}

bool PropertiesSettings::_isKnownProperty(const std::string &key) {
    return findDefinition(key) != nullptr;
}

std::string PropertiesSettings::getUnknownContents() const {
    std::map<std::string, std::string> unknown;

    for (const auto &entry: mProperties) {
        if (!_isKnownProperty(entry.first))
            unknown.insert(entry);
    }

    std::string contents("{");
    for (const auto &entry: unknown) {
        if (contents.size() > 1)
            contents += ", ";

        contents += entry.first;
        contents += "=";
        contents += entry.second;
    }

    contents += "}";
    return contents;
}

bool PropertiesSettings::hasProperty(const std::string &key) const {
    return mProperties.find(key) != mProperties.end();
}

std::string PropertiesSettings::getString(const std::string &key, const std::string &defaultValue) const {
    auto it = mProperties.find(key);
    return it == mProperties.end() ? defaultValue : it->second;
}

int PropertiesSettings::getInt(const std::string &key, int defaultValue) const {
    auto it = mProperties.find(key);
    if (it == mProperties.end() || it->second.empty())
        return defaultValue;

    return atoi(it->second.c_str());
}

float PropertiesSettings::getFloat(const std::string &key, float defaultValue) const {
    auto it = mProperties.find(key);
    if (it == mProperties.end() || it->second.empty())
        return defaultValue;

    return (float) atof(it->second.c_str());
}

bool PropertiesSettings::getBool(const std::string &key, bool defaultValue) const {
    auto it = mProperties.find(key);
    if (it == mProperties.end() || it->second.empty())
        return defaultValue;

    return it->second == "true" || it->second == "1";
}

std::string PropertiesSettings::getString(const char *key) const {
    const PropertyDefinition *definition = findDefinition(key);
    return getString(key, definition == nullptr ? std::string() : definition->mDefault);
}

int PropertiesSettings::getInt(const char *key) const {
    const PropertyDefinition *definition = findDefinition(key);
    const int fallback = definition == nullptr ? 0 : atoi(definition->mDefault);
    return getInt(key, fallback);
}

float PropertiesSettings::getFloat(const char *key) const {
    const PropertyDefinition *definition = findDefinition(key);
    const float fallback = definition == nullptr ? 0.0f : (float) atof(definition->mDefault);
    return getFloat(key, fallback);
}

bool PropertiesSettings::getBool(const char *key) const {
    const PropertyDefinition *definition = findDefinition(key);
    const bool fallback = definition != nullptr
                          && (std::string(definition->mDefault) == "true" || std::string(definition->mDefault) == "1");
    return getBool(key, fallback);
}

GameType PropertiesSettings::getGameType() const {
    const std::string value = getString("gamemode");

    if (value == "creative" || value == "1")
        return GameType::Creative;
    if (value == "adventure" || value == "2")
        return GameType::Adventure;
    if (value == "spectator" || value == "6")
        return GameType::Spectator;

    return GameType::Survival;
}

Difficulty PropertiesSettings::getDifficulty() const {
    const std::string value = getString("difficulty");

    if (value == "peaceful" || value == "0")
        return Difficulty::Peaceful;
    if (value == "normal" || value == "2")
        return Difficulty::Normal;
    if (value == "hard" || value == "3")
        return Difficulty::Hard;

    return Difficulty::Easy;
}

PlayerPermission PropertiesSettings::getDefaultPlayerPermissionLevel() const {
    const std::string value = getString("default-player-permission-level");

    if (value == "visitor")
        return PlayerPermission::Visitor;
    if (value == "operator")
        return PlayerPermission::Operator;

    return PlayerPermission::Member;
}

ContentLogLevel PropertiesSettings::getContentLogLevel() const {
    const std::string value = getString("content-log-level");

    if (value == "error")
        return ContentLogLevel::Error;
    if (value == "warning")
        return ContentLogLevel::Warning;
    if (value == "verbose")
        return ContentLogLevel::Verbose;

    return ContentLogLevel::Info;
}

NetworkSettingsPacket::CompressionAlgorithm PropertiesSettings::getCompressionAlgorithm() const {
    if (getString("compression-algorithm") == "snappy")
        return NetworkSettingsPacket::CompressionAlgorithm::Snappy;

    return NetworkSettingsPacket::CompressionAlgorithm::ZLib;
}

ChatRestrictionLevel PropertiesSettings::getChatRestrictionLevel() const {
    const std::string value = getString("chat-restriction");

    if (value == "Dropped")
        return ChatRestrictionLevel::Dropped;
    if (value == "Disabled")
        return ChatRestrictionLevel::Disabled;

    return ChatRestrictionLevel::None;
}
