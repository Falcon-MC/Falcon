#include "Server/ServerHost.h"
#include "Server/ServerPaths.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Command/ServerCommandOrigin.h"
#include "Command/ExecuteCommandOrigin.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Protocol/Packets/CommandRequestPacket.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <stdexcept>

class LevelPersistenceProbe {
public:
    static bool savedBlock(Level &level, int32_t x, int32_t y, int32_t z, const std::string &name) {
        LevelChunk chunk(x >> 4, z >> 4);
        return level.mStorage.loadChunk(chunk) && chunk.getBlock(x & 15, y, z & 15).mName == name;
    }

    static bool hasPendingSave(Level &level, int32_t x, int32_t z) {
        return level.mChunkWorker->hasPendingSave(x, z);
    }

    static bool failSave(Level &level) {
        level.mChunkWorker->waitForSaves();
        level.mStorage.close();
        level.mChunkWorker->requestSave(std::make_unique<LevelChunk>(level.getChunk(0, 0)));
        const bool failed = !level.mChunkWorker->waitForSaves();
        if (!level.mStorage.open(WorldManager::getWorldsDirectory(), level.getName(), level.getDimensionId()))
            throw std::runtime_error("Could not reopen fixture storage");
        return failed;
    }

    static void queueOverflow(Level &level, int count = 2048, bool invalidY = false) {
        for (int i = 0; i < count; ++i) {
            GeneratedBlockChange change;
            change.mX = i % 16;
            change.mZ = (i / 16) % 16;
            change.mY = invalidY ? LevelChunk::MIN_Y - 1 : 140 + i / 256;
            change.mState = BlockState("minecraft:diamond_block");
            level.mIncomingChanges.push_back(std::move(change));
        }
    }
};

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    const auto root = std::filesystem::temp_directory_path()
            / ("falcon-optimization-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto previousRoot = ServerPaths::getRoot();
    int failures = 0;
    const auto check = [&failures](const char *name, const std::function<void()> &run) {
        try {
            run();
            LOG_INFO(LogAreaID::Server, "PASS: %s", name);
        } catch (const std::exception &error) {
            ++failures;
            LOG_ERROR(LogAreaID::Server, "FAIL: %s: %s", name, error.what());
        }
    };
    try {
        std::filesystem::create_directories(root);
        ServerHost host;
        require(host.acquire(), "Could not acquire host");
        ServerPaths::setRoot(root);
        ServerHostOptions options;
        options.bindAddress = "127.0.0.1";
        options.portOverride = 0;
        options.plugins = false;
        options.properties = {{"level-name", "World"}, {"level-seed", "1"}, {"max-threads", "1"},
                              {"online-mode", "false"}, {"network-encryption", "false"},
                              {"view-distance", "4"}, {"tick-distance", "4"},
                              {"auto-compaction-interval", "0"}};
        require(host.start(options), "Could not start host");
        auto &handler = host.getHandler();
        auto &level = handler.getLevel();
        ServerCommandOrigin origin(&handler);
        const NetworkIdentifier id("regression", 1);
        auto inserted = handler.getPlayers().try_emplace(id, id, 9999, nullptr);
        ServerPlayer &player = inserted.first->second;
        player.setName("RegressionPlayer");
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setOp(true);
        player.moveToLevel(level);
        ServerActor *cow = handler.spawnActor(level, "minecraft:cow", Vector3f(0, 140, 0));
        require(cow != nullptr, "Could not spawn fixture cow");
        cow->setNameTag("Bob, Junior");

        check("shared selector count", [&]() {
            const auto players = handler.resolveTargets(origin, "@e[c=1]");
            const auto actors = handler.resolveActorTargets(origin, "@e[c=1]");
            require(players.size() + actors.size() == 1, "c=1 selected both a player and an actor");
            require(handler.resolveActorTargets(origin, "@e[type=cow,c=1]").size() == 1,
                    "Player count consumed a cow-only selector limit");
            require(handler.resolveTargets(origin, "@e[c=2]").size()
                    + handler.resolveActorTargets(origin, "@e[c=2]").size() == 2, "c=2 was not shared");
        });
        check("selector dimension context", [&]() {
            ExecuteCommandOrigin nether(origin, &player, Vector3f(), Vector3f(),
                                        &handler.getDimension(DimensionType::Nether));
            require(handler.resolveActorTargets(nether, "@e[type=cow]").empty(),
                    "Nether context selected an overworld cow");
            require(handler.resolveTargets(nether, "@e[type=player]").empty(),
                    "Nether context selected an overworld player");
        });
        check("quoted selector name", [&]() {
            require(handler.getCommands().dispatch(origin, "testfor @e[name=\"Bob, Junior\"]"),
                    "Quoted comma in entity name was split");
            require(handler.resolveActorTargets(origin, "@e[name=!\"Bob, Junior\"]").empty(),
                    "Quoted name negation was ignored");
            cow->setNameTag("Bob \"Junior\"");
            require(handler.getCommands().dispatch(origin, "testfor @e[name=\"Bob \\\"Junior\\\"\"]"),
                    "Escaped quote in entity name was lost");
            cow->setNameTag("Bob, Junior");
        });
        check("unavailable world", [&]() {
            LevelStorage locked;
            require(locked.open(WorldManager::getWorldsDirectory(), "Locked", 0), "Could not lock fixture world");
            require(handler.loadWorld("Locked", false, 1) == nullptr, "Locked world unexpectedly opened");
            require(handler.getWorlds().find("Locked") == nullptr, "Failed world remained registered");
        });
        check("paused gameplay packets", [&]() {
            handler.executeConsoleCommand("time set 1000");
            host.tick(true);
            CommandRequestPacket command;
            command.mCommand = "time set 1234";
            BinaryStream stream;
            command.writeWithHeader(stream, handler.getCodecContext());
            static_cast<NetworkHandler::Listener &>(handler).onDataReceived(id, stream.getBuffer());
            require(level.getTime() == 1000, "Gameplay packet changed paused world time");
            host.tick(false);
            static_cast<NetworkHandler::Listener &>(handler).onDataReceived(id, stream.getBuffer());
            require(level.getTime() == 1234, "Gameplay packet remained blocked after resume");
        });
        check("invalidated loaders survive only inside radius", [&]() {
            player.setPosition(Vector3f(0, 140, 0));
            for (bool teleport: {false, true}) {
                auto &state = player.getChunkStreamState();
                for (int32_t x = 100; x < 124; ++x) {
                    const int64_t hash = ChunkStreamHandler::packChunk(x, 100);
                    state.mInRadius.insert(hash);
                    player.getSentChunks().insert(hash);
                    level.registerChunkLoader(player.getRuntimeId(), x, 100);
                    ChunkStreamHandler::invalidateChunk(player, hash);
                }
                if (teleport)
                    ChunkStreamHandler::handleTeleport(handler, player);
                else
                    ChunkStreamHandler::handleViewDistanceChange(handler, player);
                for (int32_t x = 100; x < 124; ++x)
                    require(!level.unregisterChunkLoader(player.getRuntimeId(), x, 100),
                            "Pruning an invalidated chunk retained its loader");
            }
            player.getChunkStreamState() = ChunkStreamState();
            level.unregisterAllChunkLoaders(player.getRuntimeId());
            player.resetChunkStreaming();
        });
        check("chunk send budget shared across triggers", [&]() {
            auto &state = player.getChunkStreamState();
            player.setPosition(Vector3f(0, 140, 0));
            state.mLastLoaderChunk = ChunkStreamHandler::packChunk(0, 0);
            for (int32_t x = 0; x < 17; ++x) {
                level.getChunk(x, 0);
                const int64_t hash = ChunkStreamHandler::packChunk(x, 0);
                state.mInRadius.insert(hash);
                player.getSentChunks().insert(hash);
                ChunkStreamHandler::invalidateChunk(player, hash);
            }
            const size_t before = player.getSentChunkCount();
            ChunkStreamHandler::tick(handler, player);
            ChunkStreamHandler::tick(handler, player);
            require(player.getSentChunkCount() - before == 16, "Repeated calls exceeded send budget");
            host.tick(true);
            require(player.getSentChunkCount() - before == 17, "Paused network tick did not renew send budget");
            state = ChunkStreamState();
            level.unregisterAllChunkLoaders(player.getRuntimeId());
            player.resetChunkStreaming();
        });
        check("paused ticks complete terrain without advancing time", [&]() {
            handler.autoSave(true);
            LevelPersistenceProbe::queueOverflow(level);
            const int64_t time = level.getTime();
            host.tick(true);
            require(level.getBlockState(15, 143, 15).mName == "minecraft:diamond_block",
                    "Paused tick did not apply generation overflow");
            require(level.getBlockState(15, 147, 15).mName != "minecraft:diamond_block",
                    "Paused tick exceeded generation budget");
            host.tick(true);
            require(level.getBlockState(15, 147, 15).mName == "minecraft:diamond_block",
                    "Paused generation backlog did not drain");
            require(level.getTime() == time, "Terrain completion advanced paused time");
        });
        check("large generation queue drains within the change budget", [&]() {
            for (int count: {16384, 65536, 262144}) {
                Level backlog("QueueBenchmark", 4, 1, DimensionType::Overworld);
                LevelPersistenceProbe::queueOverflow(backlog, count, true);
                size_t processed = 0;
                double maxMicros = 0;
                const auto start = std::chrono::steady_clock::now();
                for (;;) {
                    const auto tickStart = std::chrono::steady_clock::now();
                    const size_t batch = backlog.processGeneratedChanges();
                    const double micros = std::chrono::duration<double, std::micro>(
                            std::chrono::steady_clock::now() - tickStart).count();
                    if (batch == 0)
                        break;
                    require(batch <= 1024, "Generation processing exceeded its change budget");
                    maxMicros = std::max(maxMicros, micros);
                    processed += batch;
                }
                require(processed == (size_t) count, "Generation queue lost pending changes");
                const double totalMicros = std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - start).count();
                LOG_INFO(LogAreaID::Server, "Queue-only backlog=%d total_us=%.1f max_batch_us=%.1f",
                         count, totalMicros, maxMicros);
            }
        });
        check("generation backlog survives close", [&]() {
            Level backlog("Backlog", 4, 1, DimensionType::Overworld);
            require(backlog.openStorage(WorldManager::getWorldsDirectory()), "Could not open fixture level");
            backlog.getChunk(0, 0);
            backlog.startWorkers(1);
            LevelPersistenceProbe::queueOverflow(backlog);
            require(backlog.processGeneratedChanges() == 1024, "Fixture did not exceed per-tick budget");
            backlog.closeStorage();
            Level reopened("Backlog", 4, 1, DimensionType::Overworld);
            require(reopened.openStorage(WorldManager::getWorldsDirectory()), "Could not reopen fixture level");
            require(reopened.getBlockState(15, 147, 15).mName == "minecraft:diamond_block",
                    "Generation backlog was lost at close");
            reopened.closeStorage();
        });
        check("explicit save completes disk writes", [&]() {
            level.getChunk(0, 0).setBlock(1, 140, 1, BlockState("minecraft:diamond_block"));
            LevelPersistenceProbe::queueOverflow(level);
            handler.autoSave(true);
            require(!LevelPersistenceProbe::hasPendingSave(level, 0, 0), "Save returned with pending writes");
            require(LevelPersistenceProbe::savedBlock(level, 15, 147, 15, "minecraft:diamond_block"),
                    "Save completed before generation backlog reached storage");
        });
        check("explicit save reports disk errors", [&]() {
            const auto metadata = root / "worlds" / "World" / "level.dat";
            std::filesystem::remove(metadata);
            std::filesystem::create_directory(metadata);
            bool rejected = false;
            try {
                handler.autoSave(true);
            } catch (const std::exception &) {
                rejected = true;
            }
            std::filesystem::remove(metadata);
            require(rejected, "Save reported success despite level.dat write failure");
            handler.autoSave(true);
        });
        check("failed async save remains retryable", [&]() {
            Level retry("Retry", 4, 1, DimensionType::Overworld);
            require(retry.openStorage(WorldManager::getWorldsDirectory()), "Could not open retry level");
            retry.getChunk(0, 0).setBlock(1, 140, 1, BlockState("minecraft:diamond_block"));
            retry.startWorkers(1);
            require(LevelPersistenceProbe::failSave(retry), "Worker ignored storage failure");
            require(LevelPersistenceProbe::hasPendingSave(retry, 0, 0), "Worker discarded failed save");
            retry.getChunk(0, 0).clearDirty();
            retry.saveAll(true);
            require(!LevelPersistenceProbe::hasPendingSave(retry, 0, 0), "Retry remained pending");
            require(LevelPersistenceProbe::savedBlock(retry, 1, 140, 1, "minecraft:diamond_block"),
                    "Retry lost the failed snapshot");
            retry.closeStorage();
        });
        handler.getPlayers().clear();
        host.stop();
        ServerPaths::setRoot(previousRoot);
        host.release();
        std::filesystem::remove_all(root);
    } catch (const std::exception &error) {
        ++failures;
        LOG_ERROR(LogAreaID::Server, "Fixture failure: %s", error.what());
        ServerPaths::setRoot(previousRoot);
    }
    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
