#pragma once

#include "Core/NBT/Tag.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class SyncWorldClocksPacket;

struct WorldClockMarker {
    std::string mName;
    int32_t mTime = 0;
    bool mHasPeriod = false;
    int32_t mPeriod = 0;
};

struct WorldClock {
    std::string mName;
    int32_t mTime = 0;
    bool mPaused = false;
    std::vector<WorldClockMarker> mMarkers;
    // markers are matched once per time value
    bool mMarkersChecked = false;
};

// the reasons the game formats in sub_145D07980 and sub_145D07DD0
enum class WorldClockError {
    None,
    TooManyClocks,
    ClockNameTaken,
    ClockNameTooLong,
    InvalidClockName,
    TooManyMarkers,
    MarkerNameTaken,
    MarkerNameTooLong,
    InvalidMarkerName,
    PeriodNotAfterTime
};

// world clocks and their time markers; one per server, shared by every pack
class WorldClockSystem {
public:
    static constexpr size_t MAX_CLOCKS = 256;
    static constexpr size_t MAX_MARKERS = 256;
    static constexpr size_t MAX_NAME_LENGTH = 128;
    // sub_145E1A5C0: every clock's state goes to every client this often
    static constexpr int64_t SYNC_INTERVAL_TICKS = 256;
    static constexpr const char *STORAGE_KEY = "WorldClocks";

    struct Listener {
        std::function<void(const WorldClock &, const WorldClockMarker &)> mTimeMarker;
        std::function<void(const WorldClock &)> mTimeModified;
        std::function<void(const WorldClock &, bool paused)> mPauseChanged;
        // true cancels: the clock stays at its maximum time
        std::function<bool(const WorldClock &, int32_t &newTime)> mRestart;
        // the network side: the clock's state, a marker added or removed
        std::function<void(const WorldClock &)> mStateChanged;
        std::function<void(const WorldClock &, const WorldClockMarker &, bool added)> mMarkerChanged;
    };

    WorldClockSystem();

    void setListener(Listener listener) {
        mListener = std::move(listener);
    }

    static std::string reasonText(WorldClockError error);

    // FNV-1 64 of the name: the HashedString the game sends as the clock or marker id
    static uint64_t idOf(const std::string &name);

    WorldClockError registerClock(const std::string &name, const std::vector<WorldClockMarker> &markers,
                                  bool &markerError);

    WorldClock *find(const std::string &name);

    const std::vector<WorldClock> &getClocks() const {
        return mClocks;
    }

    WorldClockError addMarker(WorldClock &clock, const WorldClockMarker &marker);

    const WorldClockMarker *findMarker(const WorldClock &clock, const std::string &name) const;

    void removeMarker(WorldClock &clock, const std::string &name);

    void setTime(WorldClock &clock, int32_t time);

    void setPaused(WorldClock &clock, bool paused);

    // false when the clock is behind the marker's first occurrence
    bool rewindTo(WorldClock &clock, const WorldClockMarker &marker);

    void skipTo(WorldClock &clock, const WorldClockMarker &marker);

    void tick();

    void writeRegistry(SyncWorldClocksPacket &packet) const;

    // every clock's state, as SyncState sends it
    void writeStates(SyncWorldClocksPacket &packet) const;

    // sub_145D0B870: {clocks: [{Name, Time, IsPaused}]}; markers come from registration
    Tag save() const;

    // a clock registered later takes its saved state then
    void load(const Tag &saved);

    static void writeState(const WorldClock &clock, SyncWorldClocksPacket &packet);

private:
    void _checkMarkers(WorldClock &clock);

    void _restore(WorldClock &clock) const;

    std::vector<WorldClock> mClocks;
    Listener mListener;
    Tag mSaved = Tag::ofCompound();
};
