#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/MoveActorAbsolutePacket.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ServerPlayer &join(ServerNetworkHandler &owner, Level &level, const char *name, uint64_t runtimeId,
                       const Vector3f &position) {
        const NetworkIdentifier id(name, runtimeId);
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, runtimeId).first->second;
        player.setName(name);
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setPosition(position);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        return player;
    }

    // a snowball's flight goes to the player who sees it only, and not on the ground
    void run() {
        ServerNetworkHandler owner("Projectile sync", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.setOwner(&owner);
        for (int32_t x = -2; x <= 2; ++x) {
            for (int32_t z = -2; z <= 2; ++z)
                level.insertChunk(LevelChunk(x, z)).setPopulated(true);
        }

        ServerPlayer &watcher = join(owner, level, "watcher", 7100, Vector3f(0.5f, 70.0f, 0.5f));
        for (int32_t x = -2; x <= 2; ++x) {
            for (int32_t z = -2; z <= 2; ++z)
                watcher.getSentChunks().insert(ChunkStreamHandler::packChunk(x, z));
        }
        const NetworkIdentifier watcherId = watcher.getNetworkIdentifier();
        // in the level, nowhere near the snowball
        join(owner, level, "stranger", 7101, Vector3f(5000.5f, 70.0f, 5000.5f));

        int32_t watcherMoves = 0;
        int32_t strangerMoves = 0;
        bool grounded = false;
        uint64_t snowballId = 0;
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &id, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, receiver = 0;
            const MinecraftPacketIds packetId = Packet::peekId(header, sender, receiver);
            if (packetId != MinecraftPacketIds::MoveActorAbsolute)
                return false;
            ReadOnlyBinaryStream body(data);
            MoveActorAbsolutePacket packet;
            packet.readHeader(body);
            packet.read(body, owner.getCodecContext());
            if (snowballId == 0 || packet.mRuntimeActorId != (int64_t) snowballId)
                return false;
            if (id == watcherId)
                ++watcherMoves;
            else
                ++strangerMoves;
            grounded = grounded || packet.mOnGround;
            return false;
        });

        ServerActor *snowball = owner.spawnDispensedProjectile(level, "minecraft:snowball", Vector3f(0.5f, 72.0f, 2.5f),
                                                               Vector3f(0.0f, 0.0f, 1.0f));
        require(snowball != nullptr, "no snowball was dispensed");
        snowballId = snowball->getRuntimeId();

        for (int tick = 0; tick < 3; ++tick)
            owner.tickActors();
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(watcherMoves >= 3, "the watcher missed the snowball's flight: " + std::to_string(watcherMoves));
        require(strangerMoves == 0, "a player who cannot see the snowball was sent its flight");
        require(!grounded, "a snowball in the air was sent on the ground");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-projectile-sync-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        run();
    } catch (const std::exception &error) {
        std::clog << "Projectile sync regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
