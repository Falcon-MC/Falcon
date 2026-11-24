#include "Scripting/WorldClockSystem.h"

#include "Protocol/Packets/SyncWorldClocksPacket.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cctype>
#include <climits>

namespace {
    const int32_t DAY_LENGTH = (int32_t) Level::TICKS_PER_DAY;
    const char *const OVERWORLD = "minecraft:overworld";

    // one colon, two non-empty parts, no whitespace, case-insensitive, no minecraft namespace
    bool validName(const std::string &name) {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char) std::tolower(c); });
        const size_t colon = lower.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= lower.size()
            || lower.find(':', colon + 1) != std::string::npos)
            return false;
        if (lower.find_first_of(" \t\n\r") != std::string::npos)
            return false;
        return lower.compare(0, colon, "minecraft") != 0;
    }

    WorldClockError checkMarker(const WorldClock &clock, const WorldClockMarker &marker,
                                const std::vector<WorldClockMarker> &pending) {
        if (clock.mMarkers.size() + pending.size() >= WorldClockSystem::MAX_MARKERS)
            return WorldClockError::TooManyMarkers;
        if (marker.mName.size() > WorldClockSystem::MAX_NAME_LENGTH)
            return WorldClockError::MarkerNameTooLong;
        if (!validName(marker.mName))
            return WorldClockError::InvalidMarkerName;
        const auto same = [&](const WorldClockMarker &other) { return other.mName == marker.mName; };
        if (std::any_of(clock.mMarkers.begin(), clock.mMarkers.end(), same)
            || std::any_of(pending.begin(), pending.end(), same))
            return WorldClockError::MarkerNameTaken;
        if (marker.mHasPeriod && marker.mPeriod <= marker.mTime)
            return WorldClockError::PeriodNotAfterTime;
        return WorldClockError::None;
    }

    TimeMarkerData markerData(const WorldClockMarker &marker) {
        TimeMarkerData data;
        data.mId = WorldClockSystem::idOf(marker.mName);
        data.mName = marker.mName;
        data.mTime = marker.mTime;
        data.mHasPeriod = marker.mHasPeriod;
        data.mPeriod = marker.mPeriod;
        return data;
    }
}

WorldClockSystem::WorldClockSystem() {
    // the overworld clock and its day markers, each repeating every day
    WorldClock overworld;
    overworld.mName = OVERWORLD;
    const std::pair<const char *, int32_t> markers[] = {{"minecraft:day", 1000},       {"minecraft:noon", 6000},
                                                        {"minecraft:sunset", 12000},   {"minecraft:night", 13000},
                                                        {"minecraft:midnight", 18000}, {"minecraft:sunrise", 23000}};
    for (const auto &entry: markers)
        overworld.mMarkers.push_back({entry.first, entry.second, true, DAY_LENGTH});
    mClocks.push_back(std::move(overworld));
}

std::string WorldClockSystem::reasonText(WorldClockError error) {
    switch (error) {
        case WorldClockError::TooManyClocks:
            return "Cannot have more than " + std::to_string(MAX_CLOCKS) + " world clocks.";
        case WorldClockError::ClockNameTaken:
            return "World clock name already exists.";
        case WorldClockError::ClockNameTooLong:
            return "World clock name cannot be longer than " + std::to_string(MAX_NAME_LENGTH) + ".";
        case WorldClockError::InvalidClockName:
            return "World clock has invalid or missing name.";
        case WorldClockError::TooManyMarkers:
            return "Cannot have more than " + std::to_string(MAX_MARKERS) + " time markers.";
        case WorldClockError::MarkerNameTaken:
            return "Time marker name already exists.";
        case WorldClockError::MarkerNameTooLong:
            return "Time marker name cannot be longer than " + std::to_string(MAX_NAME_LENGTH) + ".";
        case WorldClockError::InvalidMarkerName:
            return "Time marker has invalid or missing name.";
        case WorldClockError::PeriodNotAfterTime:
            return "Time marker period must be greater than its time.";
        default:
            return std::string();
    }
}

uint64_t WorldClockSystem::idOf(const std::string &name) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (char c: name) {
        hash *= 0x100000001b3ull;
        hash ^= (uint64_t) (unsigned char) c;
    }
    return hash;
}

WorldClockError WorldClockSystem::registerClock(const std::string &name, const std::vector<WorldClockMarker> &markers,
                                                bool &markerError) {
    markerError = false;
    if (mClocks.size() >= MAX_CLOCKS)
        return WorldClockError::TooManyClocks;
    if (find(name) != nullptr)
        return WorldClockError::ClockNameTaken;
    if (name.size() > MAX_NAME_LENGTH)
        return WorldClockError::ClockNameTooLong;
    if (!validName(name))
        return WorldClockError::InvalidClockName;

    WorldClock clock;
    clock.mName = name;
    std::vector<WorldClockMarker> accepted;
    for (const WorldClockMarker &marker: markers) {
        const WorldClockError error = checkMarker(clock, marker, accepted);
        if (error != WorldClockError::None) {
            markerError = true;
            return error;
        }
        accepted.push_back(marker);
    }
    clock.mMarkers = std::move(accepted);
    _restore(clock);
    mClocks.push_back(std::move(clock));
    return WorldClockError::None;
}

WorldClock *WorldClockSystem::find(const std::string &name) {
    for (WorldClock &clock: mClocks) {
        if (clock.mName == name)
            return &clock;
    }
    return nullptr;
}

WorldClockError WorldClockSystem::addMarker(WorldClock &clock, const WorldClockMarker &marker) {
    const WorldClockError error = checkMarker(clock, marker, {});
    if (error != WorldClockError::None)
        return error;
    clock.mMarkers.push_back(marker);
    if (mListener.mMarkerChanged)
        mListener.mMarkerChanged(clock, marker, true);
    return WorldClockError::None;
}

const WorldClockMarker *WorldClockSystem::findMarker(const WorldClock &clock, const std::string &name) const {
    for (const WorldClockMarker &marker: clock.mMarkers) {
        if (marker.mName == name)
            return &marker;
    }
    return nullptr;
}

void WorldClockSystem::removeMarker(WorldClock &clock, const std::string &name) {
    const auto found = std::find_if(clock.mMarkers.begin(), clock.mMarkers.end(),
                                    [&](const WorldClockMarker &marker) { return marker.mName == name; });
    if (found == clock.mMarkers.end())
        return;
    const WorldClockMarker removed = *found;
    clock.mMarkers.erase(found);
    if (mListener.mMarkerChanged)
        mListener.mMarkerChanged(clock, removed, false);
}

void WorldClockSystem::_checkMarkers(WorldClock &clock) {
    if (clock.mMarkersChecked)
        return;
    clock.mMarkersChecked = true;

    // a marker hits at its time, or every period at time modulo period
    const std::vector<WorldClockMarker> markers = clock.mMarkers;
    for (const WorldClockMarker &marker: markers) {
        const bool hit = clock.mTime == marker.mTime
                         || (marker.mHasPeriod && marker.mPeriod > 0
                             && marker.mTime == (clock.mTime % marker.mPeriod + marker.mPeriod) % marker.mPeriod);
        if (hit && mListener.mTimeMarker)
            mListener.mTimeMarker(clock, marker);
    }
}

void WorldClockSystem::bindOverworld(LevelTime time) {
    mLevelTime = std::move(time);
    WorldClock *overworld = find(OVERWORLD);
    if (overworld == nullptr || !mLevelTime.mGet)
        return;
    overworld->mTime = (int32_t) std::min<int64_t>(mLevelTime.mGet(), INT32_MAX);
    overworld->mMarkersChecked = false;
    if (mLevelTime.mSetPaused)
        mLevelTime.mSetPaused(overworld->mPaused);
}

bool WorldClockSystem::_isBound(const WorldClock &clock) const {
    return mLevelTime.mGet && clock.mName == OVERWORLD;
}

void WorldClockSystem::_tickBound(WorldClock &clock) {
    // the level steps the time: commands and sleep move it too
    const int64_t time = mLevelTime.mGet();
    if (time > INT32_MAX) {
        int32_t newTime = 0;
        if (mListener.mRestart && mListener.mRestart(clock, newTime)) {
            mLevelTime.mSet(INT32_MAX);
            return;
        }
        mLevelTime.mSet(std::max(0, newTime));
        setTime(clock, std::max(0, newTime));
        return;
    }
    if (time == clock.mTime) {
        _checkMarkers(clock);
        return;
    }
    const bool jumped = time != (int64_t) clock.mTime + 1;
    clock.mTime = (int32_t) time;
    clock.mMarkersChecked = false;
    if (jumped) {
        if (mListener.mTimeModified)
            mListener.mTimeModified(clock);
        if (mListener.mStateChanged)
            mListener.mStateChanged(clock);
    }
    _checkMarkers(clock);
}

void WorldClockSystem::setTime(WorldClock &clock, int32_t time) {
    if (clock.mTime == time)
        return;
    clock.mTime = time;
    if (_isBound(clock))
        mLevelTime.mSet(time);
    if (mListener.mTimeModified)
        mListener.mTimeModified(clock);
    if (mListener.mStateChanged)
        mListener.mStateChanged(clock);
    clock.mMarkersChecked = false;
    _checkMarkers(clock);
}

void WorldClockSystem::setPaused(WorldClock &clock, bool paused) {
    if (clock.mPaused == paused)
        return;
    clock.mPaused = paused;
    if (_isBound(clock) && mLevelTime.mSetPaused)
        mLevelTime.mSetPaused(paused);
    if (mListener.mPauseChanged)
        mListener.mPauseChanged(clock, paused);
    if (mListener.mStateChanged)
        mListener.mStateChanged(clock);
}

bool WorldClockSystem::rewindTo(WorldClock &clock, const WorldClockMarker &marker) {
    // the latest occurrence at or before the current time
    const int32_t time = clock.mTime;
    if (time < marker.mTime)
        return false;

    int32_t target = marker.mTime;
    if (marker.mHasPeriod && marker.mPeriod > 0) {
        const int32_t phase = time % marker.mPeriod;
        if (marker.mTime == phase)
            target = time;
        else if (marker.mTime < phase)
            target = time + marker.mTime - phase;
        else
            target = time + marker.mTime - (phase + marker.mPeriod);
        if (target < 0)
            return false;
    }
    setTime(clock, target);
    return true;
}

void WorldClockSystem::skipTo(WorldClock &clock, const WorldClockMarker &marker) {
    // the next occurrence at or after the current time, the first one past the maximum
    int64_t target = marker.mTime;
    const int64_t time = clock.mTime;
    if (time >= 0 && marker.mHasPeriod && marker.mPeriod > 0) {
        const int64_t phase = time % marker.mPeriod;
        if (phase < marker.mTime)
            target = time + marker.mTime - phase;
        else if (phase > marker.mTime)
            target = time + marker.mPeriod + marker.mTime - phase;
        else
            target = time;
        if (target > INT32_MAX)
            target = marker.mTime;
    }
    setTime(clock, (int32_t) target);
}

void WorldClockSystem::tick() {
    for (WorldClock &clock: mClocks) {
        if (clock.mPaused)
            continue;
        if (_isBound(clock)) {
            _tickBound(clock);
            continue;
        }

        // markers of the current time first, then the step or the restart at the maximum
        _checkMarkers(clock);
        if (clock.mTime != INT32_MAX) {
            ++clock.mTime;
            clock.mMarkersChecked = false;
            continue;
        }

        int32_t newTime = 0;
        if (mListener.mRestart && mListener.mRestart(clock, newTime))
            continue;
        newTime = std::max(0, newTime);
        clock.mTime = newTime;
        clock.mMarkersChecked = false;
        if (newTime != INT32_MAX) {
            if (mListener.mTimeModified)
                mListener.mTimeModified(clock);
            if (mListener.mStateChanged)
                mListener.mStateChanged(clock);
        }
    }
}

void WorldClockSystem::writeRegistry(SyncWorldClocksPacket &packet) const {
    packet.mType = SyncWorldClocksType::InitializeRegistry;
    for (const WorldClock &clock: mClocks) {
        WorldClockData data;
        data.mId = idOf(clock.mName);
        data.mName = clock.mName;
        data.mTime = clock.mTime;
        data.mPaused = clock.mPaused;
        for (const WorldClockMarker &marker: clock.mMarkers)
            data.mTimeMarkers.push_back(markerData(marker));
        packet.mClocks.push_back(std::move(data));
    }
}

void WorldClockSystem::writeState(const WorldClock &clock, SyncWorldClocksPacket &packet) {
    packet.mType = SyncWorldClocksType::SyncState;
    SyncWorldClockStateData state;
    state.mClockId = idOf(clock.mName);
    state.mTime = clock.mTime;
    state.mPaused = clock.mPaused;
    packet.mClockStates.push_back(state);
}

void WorldClockSystem::writeStates(SyncWorldClocksPacket &packet) const {
    for (const WorldClock &clock: mClocks)
        writeState(clock, packet);
}

Tag WorldClockSystem::save() const {
    Tag clocks = Tag::ofList(Tag::Type::Compound);
    for (const WorldClock &clock: mClocks) {
        Tag entry = Tag::ofCompound();
        entry.putString("Name", clock.mName);
        entry.putInt("Time", clock.mTime);
        entry.putBool("IsPaused", clock.mPaused);
        clocks.addToList(entry);
    }
    Tag root = Tag::ofCompound();
    root.put("clocks", clocks);
    return root;
}

void WorldClockSystem::load(const Tag &saved) {
    mSaved = saved;
    for (WorldClock &clock: mClocks)
        _restore(clock);
}

void WorldClockSystem::_restore(WorldClock &clock) const {
    const Tag *clocks = mSaved.get("clocks");
    if (clocks == nullptr || !clocks->isList())
        return;
    for (const Tag &entry: clocks->getList()) {
        const Tag *name = entry.get("Name");
        if (name == nullptr || name->asString() != clock.mName)
            continue;
        const Tag *time = entry.get("Time");
        const Tag *paused = entry.get("IsPaused");
        // a bound clock keeps the level's own saved time
        if (time != nullptr && !_isBound(clock))
            clock.mTime = std::max(0, time->asInt());
        if (paused != nullptr)
            clock.mPaused = paused->asByte() != 0;
        clock.mMarkersChecked = false;
    }
}
