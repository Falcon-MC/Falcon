#include "Server/PropertiesSettings.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <vector>

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

    file << "server-name=Falcon Server\n";
    file << "gamemode=survival\n";
    file << "force-gamemode=false\n";
    file << "difficulty=easy\n";
    file << "allow-cheats=true\n";
    file << "max-players=10\n";
    file << "online-mode=true\n";
    file << "allow-list=false\n";
    file << "server-port=19132\n";
    file << "server-portv6=19133\n";
    file << "enable-lan-visibility=true\n";
    file << "view-distance=32\n";
    file << "tick-distance=4\n";
    file << "player-idle-timeout=30\n";
    file << "level-name=Bedrock level\n";
    file << "level-seed=\n";
    file << "default-player-permission-level=member\n";
    file << "texturepack-required=false\n";
    file << "content-log-file-enabled=false\n";
    file << "compression-algorithm=zlib\n";
    file << "client-side-chunk-generation-enabled=false\n";
    file << "block-network-ids-are-hashes=true\n";
    file << "disable-custom-skins=false\n";
    file << "transport=raknet\n";
}

TransportLayer PropertiesSettings::getTransportLayer() const {
    const std::string value = getString("transport", "raknet");

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
    static const std::set<std::string> known = {
            "server-name", "gamemode", "force-gamemode", "difficulty", "allow-cheats", "max-players",
            "online-mode", "allow-list", "server-port", "server-portv6",
            "enable-lan-visibility", "view-distance", "tick-distance", "player-idle-timeout", "max-threads", "autosave-interval",
            "level-name", "level-seed", "default-player-permission-level", "texturepack-required",
            "content-log-file-enabled", "content-log-console-output-enabled", "content-log-level",
            "compression-threshold", "compression-algorithm", "chat-restriction", "disable-player-interaction",
            "client-side-chunk-generation-enabled", "block-network-ids-are-hashes", "disable-custom-skins",
            "skin-change-cooldown", "spawn-protection",
            "server-authoritative-movement-strict",
            "server-authoritative-dismount-strict", "server-authoritative-entity-interactions-strict",
            "server-authoritative-block-breaking-pick-range-scalar", "server-build-radius-ratio",
            "player-position-acceptance-threshold", "player-movement-action-direction-threshold",
            "allow-inbound-script-debugging", "allow-outbound-script-debugging", "script-debugger-auto-attach",
            "disable-persona", "transport"
    };

    return known.find(key) != known.end();
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

GameType PropertiesSettings::getGameType() const {
    const std::string value = getString("gamemode", "survival");

    if (value == "creative" || value == "1")
        return GameType::Creative;
    if (value == "adventure" || value == "2")
        return GameType::Adventure;
    if (value == "spectator" || value == "6")
        return GameType::Spectator;

    return GameType::Survival;
}

Difficulty PropertiesSettings::getDifficulty() const {
    const std::string value = getString("difficulty", "easy");

    if (value == "peaceful" || value == "0")
        return Difficulty::Peaceful;
    if (value == "normal" || value == "2")
        return Difficulty::Normal;
    if (value == "hard" || value == "3")
        return Difficulty::Hard;

    return Difficulty::Easy;
}

PlayerPermission PropertiesSettings::getDefaultPlayerPermissionLevel() const {
    const std::string value = getString("default-player-permission-level", "member");

    if (value == "visitor")
        return PlayerPermission::Visitor;
    if (value == "operator")
        return PlayerPermission::Operator;

    return PlayerPermission::Member;
}

ContentLogLevel PropertiesSettings::getContentLogLevel() const {
    const std::string value = getString("content-log-level", "info");

    if (value == "error")
        return ContentLogLevel::Error;
    if (value == "warning")
        return ContentLogLevel::Warning;
    if (value == "verbose")
        return ContentLogLevel::Verbose;

    return ContentLogLevel::Info;
}

NetworkSettingsPacket::CompressionAlgorithm PropertiesSettings::getCompressionAlgorithm() const {
    if (getString("compression-algorithm", "zlib") == "snappy")
        return NetworkSettingsPacket::CompressionAlgorithm::Snappy;

    return NetworkSettingsPacket::CompressionAlgorithm::ZLib;
}

ChatRestrictionLevel PropertiesSettings::getChatRestrictionLevel() const {
    const std::string value = getString("chat-restriction", "None");

    if (value == "Dropped")
        return ChatRestrictionLevel::Dropped;
    if (value == "Disabled")
        return ChatRestrictionLevel::Disabled;

    return ChatRestrictionLevel::None;
}
