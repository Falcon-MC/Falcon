#pragma once

#include <string>
#include <utility>
#include <vector>

// a sound declared in a pack's server sound definitions; the optional fields keep whether they were declared
struct ScriptSoundDefinition {
    std::string mEventId;
    bool mHasDuration = false;
    double mDuration = 0.0;
    // duration_info mode real_time: wall clock seconds; game_time: server ticks
    bool mRealTime = false;
    bool mHasMusic = false;
    bool mHasArtist = false, mHasTitle = false, mHasGenres = false, mHasMoods = false;
    std::string mArtist, mTitle;
    std::vector<std::string> mGenres, mMoods;
    bool mHasTags = false;
    std::vector<std::pair<std::string, std::vector<std::string>>> mTags;
};

// what SoundDefinitionRegistry.getDefinitions narrows by; empty lists and unset bounds do not constrain
struct ScriptSoundFilter {
    std::vector<std::string> mArtists, mTitles, mGenres, mMoods;
    std::vector<std::pair<std::string, std::vector<std::string>>> mTags;
    bool mHasMinDuration = false, mHasMaxDuration = false;
    double mMinDuration = 0.0, mMaxDuration = 0.0;
};

// world.soundDefinitionRegistry: the sounds/sound_definitions.json documents of every pack
class ScriptSoundRegistry {
public:
    // namespace:name; a name without one is in minecraft
    static std::string normalize(const std::string &eventId);

    // a pack's document; the first pack declaring an event keeps it
    void addPack(const std::string &packRoot);

    // nullptr when no pack declares the event
    const ScriptSoundDefinition *find(const std::string &eventId) const;

    std::vector<const ScriptSoundDefinition *> definitions(const ScriptSoundFilter &filter) const;

private:
    std::vector<ScriptSoundDefinition> mDefinitions;
};
