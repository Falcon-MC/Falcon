#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class ProfilerSection : int {
    ConsoleCommands = 0,
    NetworkDrain = 1,
    ChunkDrain = 2,
    Fluids = 3,
    Players = 4,
    ChunkStreaming = 5,
    BlockActors = 6,
    ItemActors = 7,
    Announcement = 8,
    FluidBroadcast = 9,
    Redstone = 10,
    ActorPhysics = 11,
    ActorEnvironment = 12,
    ActorProjectiles = 13,
    ActorPersistence = 14,
    SkyLight = 15,
    Weather = 16,
    NetworkPumpIo = 17,
    NetworkConnection = 18,
    NetworkDecode = 19,
    NetworkHandlePacket = 20,
    Fire = 21,
    ChunkPopulation = 22,
    Count = 23
};

const char *getProfilerSectionName(ProfilerSection section);

class Profiler;

class ProfilerScopedSection {
public:
    ProfilerScopedSection(Profiler &profiler, ProfilerSection section, bool active);

    ~ProfilerScopedSection();

private:
    Profiler &mProfiler;
    ProfilerSection mSection;
    bool mActive;
};

struct ProfilerPlayerStats {
    std::string mName;
    double mTotalMs = 0.0;
    double mPeakMs = 0.0;
    uint64_t mTicks = 0;
    uint64_t mChunksSent = 0;
};

struct ProfilerTickSample {
    int64_t mTick = 0;
    double mTotalMs = 0.0;
    double mSectionMs[(int) ProfilerSection::Count] = {};
    uint32_t mPlayerCount = 0;
    uint32_t mLoadedChunks = 0;
    uint32_t mPendingChunkTasks = 0;
    uint32_t mFluidProcessed = 0;
    uint32_t mFluidScheduled = 0;
};

class Profiler {
public:
    static const size_t MAX_SAMPLES = 24000;

    bool isActive() const { return mActive; }

    void start(int64_t tick);

    bool stop(const std::string &directory, std::string &outPath, std::string &outError);

    void beginTick(int64_t tick);

    void endTick(uint32_t playerCount, uint32_t loadedChunks, uint32_t pendingChunkTasks,
                 uint32_t fluidProcessed, uint32_t fluidScheduled);

    void beginSection(ProfilerSection section);

    void endSection(ProfilerSection section);

    void recordPlayer(const std::string &name, double milliseconds, uint32_t chunksSent);

    size_t getSampleCount() const { return mSamples.size(); }

    int64_t getStartTick() const { return mStartTick; }

private:
    std::string _buildReport() const;

    bool mActive = false;
    bool mTickStarted = false;
    int64_t mStartTick = 0;
    std::chrono::steady_clock::time_point mStartTime;
    std::chrono::steady_clock::time_point mStopTime;

    std::chrono::steady_clock::time_point mTickStart;
    std::chrono::steady_clock::time_point mSectionStart[(int) ProfilerSection::Count];
    ProfilerTickSample mCurrent;

    std::vector<ProfilerTickSample> mSamples;
    std::unordered_map<std::string, ProfilerPlayerStats> mPlayers;
};
