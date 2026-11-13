#include "Scripting/ScriptSoundRegistry.h"

#include "Core/Json/Json.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
    // the document's payload member, from the 26.60 strings
    const char *const DOCUMENT_KEY = "minecraft:server_sound_definitions";

    std::string readFile(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

    std::string lower(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char character) { return (char) std::tolower(character); });
        return text;
    }

    bool equalsIgnoreCase(const std::string &left, const std::string &right) {
        return lower(left) == lower(right);
    }

    // a single string declares a one-value list
    std::vector<std::string> strings(const json::Value *value) {
        std::vector<std::string> result;
        if (value == nullptr)
            return result;
        if (value->isString()) {
            result.push_back(value->string());
            return result;
        }
        if (!value->isArray())
            return result;
        for (const std::unique_ptr<json::Value> &element: value->mArray) {
            if (element->isString())
                result.push_back(element->string());
        }
        return result;
    }

    bool parseDefinition(const json::Value &entry, ScriptSoundDefinition &definition) {
        const json::Value *name = entry.get("sound_event_name");
        if (name == nullptr || !name->isString() || name->string().empty())
            return false;
        definition.mEventId = ScriptSoundRegistry::normalize(name->string());

        const json::Value *duration = entry.get("duration_info");
        if (duration != nullptr && duration->isObject()) {
            // mode 0 game_time, 1 real_time
            const json::Value *mode = duration->get("mode");
            definition.mRealTime = mode != nullptr && mode->isString() && mode->string() == "real_time";
            const json::Value *seconds = duration->get("seconds");
            if (seconds != nullptr && seconds->isNumber()) {
                // float seconds truncated: real_time to milliseconds, game_time to ticks
                const float declared = (float) seconds->number();
                definition.mHasDuration = true;
                definition.mDurationUnits = definition.mRealTime ? (int64_t) (float) (declared * 1000.0)
                                                                 : (int64_t) (int32_t) (float) (declared * 20.0);
            }
        }

        const json::Value *range = entry.get("broadcast_range");
        if (range != nullptr && range->isNumber()) {
            definition.mHasBroadcastRange = true;
            definition.mBroadcastRange = (float) range->number();
        }

        const json::Value *music = entry.get("music_info");
        if (music != nullptr && music->isObject()) {
            definition.mHasMusic = true;
            const json::Value *artist = music->get("artist");
            if (artist != nullptr && artist->isString()) {
                definition.mHasArtist = true;
                definition.mArtist = artist->string();
            }
            const json::Value *title = music->get("title");
            if (title != nullptr && title->isString()) {
                definition.mHasTitle = true;
                definition.mTitle = title->string();
            }
            definition.mHasGenres = music->get("genres") != nullptr;
            definition.mGenres = strings(music->get("genres"));
            definition.mHasMoods = music->get("moods") != nullptr;
            definition.mMoods = strings(music->get("moods"));
        }

        const json::Value *tags = entry.get("tags");
        if (tags != nullptr && tags->isObject()) {
            definition.mHasTags = true;
            for (const std::string &key: tags->mKeys)
                definition.mTags.emplace_back(key, strings(tags->get(key)));
        }
        return true;
    }

    bool matchesOne(const std::vector<std::string> &wanted, const std::string &declared, bool hasDeclared) {
        if (wanted.empty())
            return true;
        if (!hasDeclared)
            return false;
        for (const std::string &value: wanted) {
            if (equalsIgnoreCase(value, declared))
                return true;
        }
        return false;
    }

    bool matchesAny(const std::vector<std::string> &wanted, const std::vector<std::string> &declared) {
        if (wanted.empty())
            return true;
        for (const std::string &value: wanted) {
            for (const std::string &candidate: declared) {
                if (equalsIgnoreCase(value, candidate))
                    return true;
            }
        }
        return false;
    }

    bool matchesTags(const ScriptSoundFilter &filter, const ScriptSoundDefinition &definition) {
        for (const auto &wanted: filter.mTags) {
            if (wanted.second.empty())
                continue;
            const auto declared = std::find_if(definition.mTags.begin(), definition.mTags.end(),
                                               [&](const auto &tag) { return equalsIgnoreCase(tag.first, wanted.first); });
            if (declared == definition.mTags.end() || !matchesAny(wanted.second, declared->second))
                return false;
        }
        return true;
    }
}

std::string ScriptSoundRegistry::normalize(const std::string &eventId) {
    return eventId.find(':') == std::string::npos ? "minecraft:" + eventId : eventId;
}

void ScriptSoundRegistry::addPack(const std::string &packRoot) {
    namespace fs = std::filesystem;
    const fs::path path = fs::path(packRoot) / "sounds" / "sound_definitions.json";
    std::error_code error;
    if (!fs::is_regular_file(path, error))
        return;
    const std::unique_ptr<json::Value> document = json::parse(readFile(path));
    const json::Value *entries = document == nullptr ? nullptr : document->get(DOCUMENT_KEY);
    if (entries == nullptr || !entries->isArray())
        return;
    for (const std::unique_ptr<json::Value> &entry: entries->mArray) {
        ScriptSoundDefinition definition;
        if (!entry->isObject() || !parseDefinition(*entry, definition) || find(definition.mEventId) != nullptr)
            continue;
        mDefinitions.push_back(std::move(definition));
    }
}

const ScriptSoundDefinition *ScriptSoundRegistry::find(const std::string &eventId) const {
    const std::string normalized = normalize(eventId);
    for (const ScriptSoundDefinition &definition: mDefinitions) {
        if (definition.mEventId == normalized)
            return &definition;
    }
    return nullptr;
}

std::vector<const ScriptSoundDefinition *> ScriptSoundRegistry::definitions(const ScriptSoundFilter &filter) const {
    std::vector<const ScriptSoundDefinition *> result;
    for (const ScriptSoundDefinition &definition: mDefinitions) {
        if ((filter.mHasMinDuration || filter.mHasMaxDuration) && !definition.mHasDuration)
            continue;
        // compared as floats
        if (filter.mHasMinDuration && (float) filter.mMinDuration > definition.durationSeconds())
            continue;
        if (filter.mHasMaxDuration && definition.durationSeconds() > (float) filter.mMaxDuration)
            continue;
        if (!matchesOne(filter.mArtists, definition.mArtist, definition.mHasArtist)
            || !matchesOne(filter.mTitles, definition.mTitle, definition.mHasTitle)
            || !matchesAny(filter.mGenres, definition.mGenres) || !matchesAny(filter.mMoods, definition.mMoods)
            || !matchesTags(filter, definition))
            continue;
        result.push_back(&definition);
    }
    return result;
}
