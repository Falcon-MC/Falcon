#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/EntityDataMap.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/WardenActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner, int64_t ticks = 1) {
        owner.mCurrentTick += ticks;
    }
};

namespace {
    const int32_t FLOOR_Y = 99;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Warden senses", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            mLevel.setOwner(&mOwner);
            for (int32_t x = -3; x <= 3; x++) {
                for (int32_t z = -3; z <= 3; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            const BlockState stone("minecraft:stone");
            for (int32_t x = -40; x < 40; x++) {
                for (int32_t z = -40; z < 40; z++)
                    mLevel.setBlockState(x, FLOOR_Y, z, stone);
            }
            ServerTickProbe::advance(mOwner, 1000);
        }

        WardenActor &warden(const Vector3f &position) {
            auto *warden = dynamic_cast<WardenActor *>(mOwner.spawnActor(mLevel, WardenActor::IDENTIFIER, position));
            require(warden != nullptr, "the warden did not spawn");
            return *warden;
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position, GameType gameType) {
            const NetworkIdentifier id("warden-senses-player", runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.moveToLevel(mLevel);
            player.setPosition(position);
            player.setGameType((int32_t) gameType);
            return player;
        }
    };

    Vector3f at(float x, float z) {
        return Vector3f(x, FLOOR_Y + 1.0f, z);
    }

    const EntityDataEntry *entryOf(const EntityDataMap &metadata, int32_t id) {
        for (const EntityDataEntry &entry: metadata.mEntries) {
            if (entry.mId == id)
                return &entry;
        }
        return nullptr;
    }

    // the spawn data the client times its sounds with, and how anger changes them
    void sounds() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        EntityDataMap metadata;
        warden.fillSpawnMetadata(metadata);
        const EntityDataEntry *heartbeat = entryOf(metadata, MobAmbientSounds::HEARTBEAT_INTERVAL_DATA_ID);
        require(heartbeat != nullptr && heartbeat->mIntValue == 40, "a calm heartbeat is not every 2 seconds");
        const EntityDataEntry *interval = entryOf(metadata, MobAmbientSounds::AMBIENT_INTERVAL_DATA_ID);
        const EntityDataEntry *range = entryOf(metadata, MobAmbientSounds::AMBIENT_RANGE_DATA_ID);
        const EntityDataEntry *event = entryOf(metadata, MobAmbientSounds::AMBIENT_EVENT_DATA_ID);
        require(interval != nullptr && interval->mFloatValue == 2.0f, "the ambient interval is not value");
        require(range != nullptr && range->mFloatValue == 4.0f, "the ambient range is not range");
        require(event != nullptr && event->mStringValue == "ambient", "a calm warden's ambient event is not ambient");

        ServerPlayer &player = arena.player(9200, at(30.5f, 0.5f), GameType::Survival);
        warden.getAngerLevel().increase(arena.mOwner, warden, player, 40, false);
        require(MobAmbientSounds::ambientEventOf(warden) == "agitated", "anger 40 did not make it agitated");
        require(MobAmbientSounds::heartbeatTicksOf(warden) == 25, "the heartbeat did not quicken with anger");
        warden.getAngerLevel().increase(arena.mOwner, warden, player, 40, false);
        require(MobAmbientSounds::ambientEventOf(warden) == "angry", "anger 80 did not make it angry");
        require(MobAmbientSounds::heartbeatTicksOf(warden) == 10, "the heartbeat is not every half second when angry");

        for (int32_t tick = 0; tick < 41; tick++) {
            ServerTickProbe::advance(arena.mOwner);
            warden.getAngerLevel().increase(arena.mOwner, warden, player, 1, false);
            arena.mOwner.tickActors();
        }
        require(warden.getAmbientSounds().getHeartbeatTicks() == 10, "the heartbeat data did not follow the anger");
        require(warden.getAmbientSounds().getAmbientEvent() == "angry", "the ambient event did not follow the anger");
    }

    // darkness for 13 seconds to vulnerable players within 20 blocks, every 6 seconds
    void darkness() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        ServerPlayer &near = arena.player(9201, at(15.5f, 0.5f), GameType::Survival);
        ServerPlayer &creative = arena.player(9202, at(-15.5f, 0.5f), GameType::Creative);
        ServerPlayer &far = arena.player(9203, at(0.5f, 30.5f), GameType::Survival);

        ServerTickProbe::advance(arena.mOwner);
        arena.mOwner.tickActors();
        const MobEffectInstance *effect = near.getEffect(MobEffectId::Darkness);
        require(effect != nullptr && effect->mDuration >= 259 && effect->mDuration <= 260,
                "a near player did not get 13 seconds of darkness");
        require(!creative.hasEffect(MobEffectId::Darkness), "a creative player got darkness");
        require(!far.hasEffect(MobEffectId::Darkness), "a player past effect_range got darkness");

        near.removeEffect(MobEffectId::Darkness);
        for (int32_t tick = 0; tick < 120; tick++) {
            ServerTickProbe::advance(arena.mOwner);
            arena.mOwner.tickActors();
        }
        require(!near.hasEffect(MobEffectId::Darkness), "darkness came back inside cooldown_time");
        ServerTickProbe::advance(arena.mOwner);
        arena.mOwner.tickActors();
        require(near.hasEffect(MobEffectId::Darkness), "darkness did not come back after cooldown_time");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-warden-senses-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        sounds();
        darkness();
        std::cout << "warden senses checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
