#pragma once

#include <cstdint>
#include <string>

class EntityDataMap;
class MobActor;
class ServerNetworkHandler;

// minecraft:heartbeat and minecraft:ambient_sound_interval: the client plays both sounds on its own timers,
// the server keeps the synched data they read up to date
class MobAmbientSounds {
public:
    static constexpr int32_t AMBIENT_INTERVAL_DATA_ID = 108;
    static constexpr int32_t AMBIENT_RANGE_DATA_ID = 109;
    static constexpr int32_t AMBIENT_EVENT_DATA_ID = 110;
    static constexpr int32_t HEARTBEAT_INTERVAL_DATA_ID = 125;

    void tick(ServerNetworkHandler &owner, MobActor &mob);

    void fillSpawnMetadata(const MobActor &mob, EntityDataMap &metadata) const;

    int32_t getHeartbeatTicks() const {
        return mHeartbeatTicks;
    }

    const std::string &getAmbientEvent() const {
        return mAmbientEvent;
    }

    // what the heartbeat interval evaluates to now, in ticks; 0 without the component
    static int32_t heartbeatTicksOf(const MobActor &mob);

    // the first event_names entry whose condition holds, else event_name
    static std::string ambientEventOf(const MobActor &mob);

private:
    int32_t mHeartbeatTicks = 0;
    std::string mAmbientEvent;
    int32_t mAmbientTicks = 0;
    bool mStarted = false;
};
