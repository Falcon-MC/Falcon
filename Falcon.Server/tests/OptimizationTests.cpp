#include "Server/ServerHost.h"
#include "Server/ServerPaths.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/Commands/ExecuteCommandOrigin.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Protocol/Packets/CommandRequestPacket.h"
#include "World/Level/Chunk/SubChunk.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Chunk/LightStorage.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>

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
            change.mState = SubChunk::intern(BlockState("minecraft:diamond_block"));
            level.mIncomingChanges.push_back(std::move(change));
        }
    }

    static void storeGenerated(Level &level, ChunkLoadResult &result) {
        level._storeGeneratedContent(result);
    }

    static void stagePending(Level &level, int32_t x, int32_t z, int count,
                             const char *block = "minecraft:diamond_block") {
        auto &changes = level.mPendingBlockChanges[Level::_packChunk(x, z)];
        for (int i = 0; i < count; ++i) {
            GeneratedBlockChange change;
            change.mX = x * 16 + i % 16;
            change.mZ = z * 16 + (i / 16) % 16;
            change.mY = 140 + (i / 256) % 64;
            change.mState = SubChunk::intern(BlockState(block));
            changes.push_back(std::move(change));
        }
        level.mPendingBlockChangeCount += count;
    }

    static bool writing(Level &level) {
        return level.mPendingChangeWrite.valid();
    }

    static void holdWrite(Level &level, int32_t x, int32_t z,
                          std::promise<std::vector<GeneratedBlockChange>> &completion) {
        level.mPersistingChangeKeys.insert(Level::_packChunk(x, z));
        level.mPendingChangeWrite = completion.get_future();
    }

    static size_t outstanding(Level &level) {
        return level.mChunkWorker->getOutstandingGenerationCount();
    }

    static void finishWrites(Level &level) {
        level.mChunkWorker->waitForSaves();
        level._finishPendingChangeWrite(true);
    }

    static std::vector<GeneratedBlockChange> storedPending(Level &level, int32_t x, int32_t z) {
        return level.mStorage.loadPendingBlockChanges(x, z);
    }

    static void saveEmpty(Level &level, int32_t x, int32_t z) {
        LevelChunk chunk(x, z);
        chunk.setPopulated(true);
        if (!level.mStorage.saveChunk(chunk))
            throw std::runtime_error("Could not save pressure fixture chunk");
    }

    static void failPendingWrite(Level &level, int32_t x, int32_t z) {
        level.mChunkWorker->waitForSaves();
        const int64_t key = Level::_packChunk(x, z);
        auto &changes = level.mPendingBlockChanges.at(key);
        level.mPersistingChangeKeys.insert(key);
        level.mPersistingChangeCount = changes.size();
        level.mPendingBlockChangeCount -= changes.size();
        level.mStorage.close();
        level.mPendingChangeWrite = level.mChunkWorker->requestPersistChanges(x, z, std::move(changes));
        level.mPendingBlockChanges.erase(key);
        finishWrites(level);
        if (!level.mStorage.open(WorldManager::getWorldsDirectory(), level.getName(), level.getDimensionId()))
            throw std::runtime_error("Could not reopen pending-write fixture storage");
    }
};

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // fails its first chunk, like a generator bug surfacing on a worker
    class ThrowOnceGenerator : public ChunkGenerator {
    public:
        DimensionType getDimensionType() const override {
            return DimensionType::Overworld;
        }

        void generate(LevelChunk &) const override {
            if (!mThrown) {
                mThrown = true;
                throw std::runtime_error("generation failed");
            }
        }

        void decorate(Level &, LevelChunk &, std::vector<GeneratedBlockChange> *) const override {}

        int64_t getSeed() const override {
            return 1;
        }

        int getSpawnY() const override {
            return 64;
        }

        int32_t getAirHash() const override {
            return 0;
        }

        int32_t pickBiome(int32_t, int32_t, int32_t) const override {
            return 0;
        }

    private:
        mutable bool mThrown = false;
    };
}

namespace {
    class FakePeer : public NetworkPeer {
    public:
        void sendPacket(const std::string &data, Reliability, Compressibility) override {
            mSentBytes += data.size();
        }

        DataStatus receivePacket(std::string &outData) override {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mIncoming.empty())
                return DataStatus::NoData;
            outData = std::move(mIncoming.front());
            mIncoming.pop_front();
            return DataStatus::HasData;
        }

        NetworkStatus getNetworkStatus() const override {
            return NetworkStatus();
        }

        bool usesGamePacketId() const override {
            return false;
        }

        bool encryptsGamePackets() const override {
            return false;
        }

        // batched like a client does, behind the compression header once it is on
        void deliver(const std::string &packet, bool compressed = false) {
            std::string batch;
            if (compressed)
                batch.push_back((char) CompressedNetworkPeer::CompressionAlgorithm::None);
            batch.push_back((char) packet.size());
            batch += packet;
            std::lock_guard<std::mutex> lock(mMutex);
            mIncoming.push_back(batch);
        }

        std::atomic<size_t> mSentBytes{0};

    private:
        std::mutex mMutex;
        std::deque<std::string> mIncoming;
    };

    // answers the first packet at once, like a client on a fast link
    class AnsweringPeer : public FakePeer {
    public:
        explicit AnsweringPeer(std::string reply) : mReply(std::move(reply)) {}

        void sendPacket(const std::string &data, Reliability reliability, Compressibility compressibility) override {
            FakePeer::sendPacket(data, reliability, compressibility);
            if (mReply.empty())
                return;

            deliver(mReply, true);
            mReply.clear();
        }

    private:
        std::string mReply;
    };

    class FakeConnector : public Connector {
    public:
        bool host(const ConnectionDefinition &) override {
            return true;
        }

        void disconnect() override {}

        void runEvents() override {
            std::vector<std::pair<NetworkIdentifier, std::shared_ptr<NetworkPeer>>> joining;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                joining.swap(mJoining);
            }
            for (const auto &entry: joining)
                mCallbacks->onNewIncomingConnection(entry.first, entry.second);
        }

        std::shared_ptr<NetworkPeer> getPeerForUser(const NetworkIdentifier &) override {
            return nullptr;
        }

        bool closeConnection(const NetworkIdentifier &id) override {
            std::lock_guard<std::mutex> lock(mMutex);
            mClosed.push_back(id);
            return true;
        }

        TransportLayer getNetworkType() const override {
            return TransportLayer::NetherNet;
        }

        bool isIPv4Supported() const override {
            return true;
        }

        bool isIPv6Supported() const override {
            return false;
        }

        void join(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer) {
            std::lock_guard<std::mutex> lock(mMutex);
            mJoining.emplace_back(id, std::move(peer));
        }

        size_t closedCount() {
            std::lock_guard<std::mutex> lock(mMutex);
            return mClosed.size();
        }

    private:
        std::mutex mMutex;
        std::vector<std::pair<NetworkIdentifier, std::shared_ptr<NetworkPeer>>> mJoining;
        std::vector<NetworkIdentifier> mClosed;
    };

    class RecordingListener : public NetworkHandler::Listener {
    public:
        void onNewIncomingConnection(const NetworkIdentifier &) override {
            mEvents.push_back("open");
        }

        void onConnectionClosed(const NetworkIdentifier &, DisconnectFailReason reason, const std::string &) override {
            mEvents.push_back(reason == DisconnectFailReason::ConnectionRefused ? "refused" : "closed");
        }

        void onDataReceived(const NetworkIdentifier &, const std::string &data) override {
            mEvents.push_back("data:" + data);
        }

        std::vector<std::string> mEvents;
    };

    template <typename Predicate>
    void pumpUntil(NetworkHandler &network, Predicate done, const char *message) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (!done()) {
            require(std::chrono::steady_clock::now() < deadline, message);
            network.runEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    const auto root = std::filesystem::temp_directory_path()
                      / ("falcon-optimization-"
                         + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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
        options.properties = {{"level-name", "World"},
                              {"level-seed", "1"},
                              {"max-threads", "1"},
                              {"online-mode", "false"},
                              {"network-encryption", "false"},
                              {"view-distance", "4"},
                              {"tick-distance", "4"},
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
                                    + handler.resolveActorTargets(origin, "@e[c=2]").size()
                            == 2,
                    "c=2 was not shared");
        });
        check("selector dimension context", [&]() {
            ExecuteCommandOrigin nether(origin, &player, Vector3f(), Vector3f(),
                                        &handler.getDimension(DimensionType::Nether));
            require(handler.resolveActorTargets(nether, "@e[type=cow]").empty(),
                    "Nether context selected an overworld cow");
            require(handler.resolveTargets(nether, "@e[type=player]").empty(),
                    "Nether context selected an overworld player");
            require(handler.resolveTargets(nether, "@a").size() == 1, "@a left out a player of another dimension");
            World *other = handler.loadWorld("SelectorWorld", true, 1);
            require(other != nullptr, "Could not load the second world");
            ExecuteCommandOrigin elsewhere(origin, &player, Vector3f(), Vector3f(), &other->getOverworld());
            require(handler.resolveTargets(elsewhere, "@a").empty() && handler.resolveTargets(elsewhere, "@r").empty(),
                    "A command of another world selected this world's player");
            std::string error;
            require(handler.unloadWorld("SelectorWorld", error), "Could not unload the second world");
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
        check("a decoration that lands on a loaded chunk joins its loaded actors", [&]() {
            const int64_t column = ((int64_t) 41 << 32) | (uint32_t) 41;
            std::unordered_set<int64_t> &loaded =
                    handler.getWorldOf(level).getActorLoadedChunks(level.getDimensionType());
            loaded.insert(column);
            level.getBlockActors().loadChunk(41, 41, {}, handler.getCodecContext());

            ChunkLoadResult result;
            result.mX = 41;
            result.mZ = 41;
            Tag chest = Tag::ofCompound();
            chest.putString("id", "Chest");
            chest.putInt("x", 664);
            chest.putInt("y", 70);
            chest.putInt("z", 664);
            result.mBlockActors.push_back(chest);
            Tag position = Tag::ofList(Tag::Type::Float);
            position.addToList(Tag::ofFloat(664.5f));
            position.addToList(Tag::ofFloat(70.0f));
            position.addToList(Tag::ofFloat(665.5f));
            Tag pig = Tag::ofCompound();
            pig.putString("identifier", "minecraft:pig");
            pig.put("Pos", position);
            result.mEntities.push_back(pig);
            LevelPersistenceProbe::storeGenerated(level, result);

            int64_t spawned = 0;
            for (auto &entry: handler.getActors()) {
                const Vector3f at = entry.second->getPosition();
                if (entry.second->isIn(level) && at.x == 664.5f && at.z == 665.5f)
                    spawned = entry.first;
            }
            bool stored = false;
            for (const Tag &entity: level.loadEntities(41, 41))
                stored = stored || (spawned != 0 && entity.getLong("UniqueID", 0) == spawned);
            const bool chestLoaded = level.getBlockActors().find(Vector3i(664, 70, 664)) != nullptr;

            if (spawned != 0)
                handler.removeActor(spawned);
            level.getBlockActors().unloadChunk(41, 41);
            loaded.erase(column);
            require(chestLoaded, "A generated chest did not join the loaded block actors");
            require(spawned != 0, "A generated mob did not join the loaded actors");
            require(stored, "A generated mob was not stored with its unique id");
        });
        check("rights commands take the whole player name", [&]() {
            require(handler.getPlayerByName("Regression") == &player, "A name prefix no longer finds the player");
            require(handler.getPlayerByExactName("regressionplayer") == &player,
                    "The whole name in another case missed");
            require(handler.getPlayerByExactName("Regression") == nullptr, "A name prefix found a player for rights");
            handler.getCommands().dispatch(origin, "deop Regression");
            require(player.isOp(), "Deop of a name prefix took the rights of another player");
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
        check("a teleport streams the whole view and keeps the chunks still in it", [&]() {
            auto &state = player.getChunkStreamState();
            player.setPosition(Vector3f(0, 140, 0));
            const int64_t kept = ChunkStreamHandler::packChunk(2, 0);
            state.mInRadius.insert(kept);
            player.getSentChunks().insert(kept);
            level.registerChunkLoader(player.getRuntimeId(), 2, 0);
            player.setPosition(Vector3f(8, 140, 8));
            ChunkStreamHandler::handleTeleport(handler, player);
            require(player.getSentChunks().count(kept) == 1, "A short teleport dropped a chunk still in view");
            const auto afterTeleport = state.mInRadius;
            ChunkStreamHandler::handleViewDistanceChange(handler, player);
            require(state.mInRadius == afterTeleport, "A teleport did not stream the whole view distance");
            state = ChunkStreamState();
            level.unregisterAllChunkLoaders(player.getRuntimeId());
            player.resetChunkStreaming();
        });
        check("chunk send budget shared across triggers", [&]() {
            auto &state = player.getChunkStreamState();
            // a player waiting for its spawn chunks gets them all in one tick; the tick drops the join burst
            player.setSpawnChunksReady(true);
            host.tick(true);
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
            const size_t after = player.getSentChunkCount() - before;
            require(after > 16, "Paused network tick did not renew send budget");
            require(after <= 32, "Paused network tick exceeded send budget after terrain invalidation");
            state = ChunkStreamState();
            level.unregisterAllChunkLoaders(player.getRuntimeId());
            player.resetChunkStreaming();
        });
        check("spawn chunks sent in one tick", [&]() {
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
            require(player.getSentChunkCount() - before == 17, "Spawn chunks waited for the steady budget");
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
                    const double micros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now()
                                                                                    - tickStart)
                                                  .count();
                    if (batch == 0)
                        break;
                    require(micros < 50000.0, "Generation processing overran a tick");
                    maxMicros = std::max(maxMicros, micros);
                    processed += batch;
                }
                require(processed == (size_t) count, "Generation queue lost pending changes");
                const double totalMicros =
                        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
                LOG_INFO(LogAreaID::Server, "Queue-only backlog=%d total_us=%.1f max_batch_us=%.1f", count, totalMicros,
                         maxMicros);
            }
        });
        check("pending changes persist off the tick path and precede synchronous loads", [&]() {
            Level pending("PendingAsync", 4, 1);
            require(pending.openStorage(WorldManager::getWorldsDirectory()), "Could not open pending fixture");
            pending.startWorkers(1);
            LevelPersistenceProbe::stagePending(pending, 32, 32, 16384);
            require(pending.processGeneratedChanges() == 0, "Persistence changed the generation budget");
            require(!LevelPersistenceProbe::writing(pending), "A few parked changes were written on a tick");
            const auto start = std::chrono::steady_clock::now();
            pending.saveAll(false);
            const double queuedMicros =
                    std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
            require(LevelPersistenceProbe::writing(pending), "Save did not hand persistence to a worker");
            require(pending.getBlockState(32 * 16, 140, 32 * 16).mName == "minecraft:diamond_block",
                    "Synchronous load missed the pending write");
            LevelPersistenceProbe::stagePending(pending, 35, 35, 1, "minecraft:glowstone");
            pending.processGeneratedChanges();
            LevelPersistenceProbe::finishWrites(pending);
            pending.requestChunkAsync(35, 35);
            LevelPersistenceProbe::finishWrites(pending);
            pending.drainCompletedChunks();
            require(pending.getBlockState(35 * 16, 140, 35 * 16).mName == "minecraft:glowstone",
                    "Asynchronous load missed persisted changes");
            require(pending.getChunk(35, 35).getBlockLight(0, 140, 0) > 0, "Persisted light source was not relit");
            pending.closeStorage();
            Level reopened("PendingAsync", 4, 1);
            require(reopened.openStorage(WorldManager::getWorldsDirectory()), "Could not reopen pending fixture");
            require(reopened.getBlockState(32 * 16 + 15, 203, 32 * 16 + 15).mName == "minecraft:diamond_block",
                    "Pending write lost blocks across close");
            reopened.closeStorage();
            LOG_INFO(LogAreaID::Server, "Async persistence handoff: changes=16384 main_thread_us=%.1f", queuedMicros);
        });
        check("streaming defers a load while persistence is unfinished", [&]() {
            Level gate("WriteGate", 4, 1);
            gate.startWorkers(1);
            std::promise<std::vector<GeneratedBlockChange>> completion;
            LevelPersistenceProbe::holdWrite(gate, 37, 37, completion);
            gate.requestChunkAsync(37, 37);
            const size_t outstanding = LevelPersistenceProbe::outstanding(gate);
            completion.set_value({});
            require(outstanding == 0, "Load was queued before persistence completed");
            gate.closeStorage();
        });
        check("failed pending write retains changes and newest states win", [&]() {
            Level pending("PendingRetry", 4, 1);
            require(pending.openStorage(WorldManager::getWorldsDirectory()), "Could not open pending retry fixture");
            pending.startWorkers(1);
            LevelPersistenceProbe::stagePending(pending, 33, 33, 1024);
            LevelPersistenceProbe::failPendingWrite(pending, 33, 33);
            require(pending.getPendingChangeChunkCount() == 1, "Failed persistence discarded its payload");
            LevelPersistenceProbe::stagePending(pending, 33, 33, 1, "minecraft:gold_block");
            pending.saveAll(false);
            LevelPersistenceProbe::finishWrites(pending);
            const auto saved = LevelPersistenceProbe::storedPending(pending, 33, 33);
            require(saved.size() == 1024, "Retry lost or duplicated positions");
            const auto newest = std::find_if(saved.begin(), saved.end(), [](const GeneratedBlockChange &change) {
                return change.mX == 33 * 16 && change.mY == 140 && change.mZ == 33 * 16;
            });
            require(newest != saved.end() && newest->mState->mName == "minecraft:gold_block",
                    "Retry overwrote a newer state");
            LevelPersistenceProbe::stagePending(pending, 34, 34, 1024);
            pending.processGeneratedChanges();
            pending.closeStorage();
            Level reopened("PendingRetry", 4, 1);
            require(reopened.openStorage(WorldManager::getWorldsDirectory()), "Could not reopen pending retry fixture");
            require(reopened.getBlockState(34 * 16, 140, 34 * 16).mName == "minecraft:diamond_block",
                    "Shutdown discarded a pending write");
            reopened.closeStorage();
        });
        check("generation admission pauses under pressure and resumes without losing requests", [&]() {
            Level pressure("Pressure", 4, 1);
            require(pressure.openStorage(WorldManager::getWorldsDirectory()), "Could not open pressure fixture");
            for (int x = 40; x < 64; ++x)
                LevelPersistenceProbe::saveEmpty(pressure, x, 40);
            pressure.startWorkers(1);
            LevelPersistenceProbe::queueOverflow(pressure, 65536, true);
            for (int x = 40; x < 64; ++x)
                pressure.requestChunkAsync(x, 40);
            require(LevelPersistenceProbe::outstanding(pressure) == 0, "Generation ignored backlog pressure");
            while (pressure.processGeneratedChanges() != 0) {
            }
            for (int x = 40; x < 64; ++x)
                pressure.requestChunkAsync(x, 40);
            require(LevelPersistenceProbe::outstanding(pressure)
                            == std::min<size_t>(24, ChunkWorker::MAX_OUTSTANDING_GENERATION),
                    "Worker admission did not enforce its outstanding-result limit");
            for (int round = 0; round < 3; ++round) {
                LevelPersistenceProbe::finishWrites(pressure);
                pressure.drainCompletedChunks();
                for (int x = 40; x < 64; ++x)
                    pressure.requestChunkAsync(x, 40);
                require(LevelPersistenceProbe::outstanding(pressure) <= ChunkWorker::MAX_OUTSTANDING_GENERATION,
                        "Generation exceeded its result credit limit");
            }
            LevelPersistenceProbe::finishWrites(pressure);
            pressure.drainCompletedChunks();
            for (int x = 40; x < 64; ++x)
                require(pressure.isChunkPopulated(x, 40), "Deferred generation request never completed");
            pressure.closeStorage();
        });
        check("ticking areas retry chunks deferred by generation pressure", [&]() {
            World &world = handler.getWorldFor(player);
            for (int x = 70; x < 86; ++x)
                LevelPersistenceProbe::saveEmpty(level, x, 70);
            auto area = TickingArea::box(Vector3i(70 * 16, 0, 70 * 16), Vector3i(85 * 16, 0, 70 * 16));
            const std::string areaId = world.getTickingAreas().add(world, area).mId;
            for (int round = 0; round < 8; ++round) {
                LevelPersistenceProbe::finishWrites(level);
                host.tick(true);
            }
            for (int x = 70; x < 86; ++x)
                require(level.isChunkPopulated(x, 70), "Ticking area never retried deferred terrain");
            world.getTickingAreas().remove(world, [&areaId](const TickingArea &entry) { return entry.mId == areaId; });
        });
        check("parked changes of a load discarded at close survive", [&]() {
            Level parked("ParkedClose", 4, 1);
            require(parked.openStorage(WorldManager::getWorldsDirectory()), "Could not open parked close fixture");
            parked.startWorkers(1);
            // fresh terrain keeps the only worker busy: the last load is still queued when close discards it
            for (int x = 90; x < 94; ++x)
                parked.requestChunkAsync(x, 90);
            LevelPersistenceProbe::stagePending(parked, 95, 90, 256, "minecraft:gold_block");
            parked.requestChunkAsync(95, 90);
            require(parked.getPendingChangeChunkCount() == 0, "Load did not take its parked changes");
            parked.closeStorage();
            Level reopened("ParkedClose", 4, 1);
            require(reopened.openStorage(WorldManager::getWorldsDirectory()), "Could not reopen parked close fixture");
            require(reopened.getBlockState(95 * 16, 140, 90 * 16).mName == "minecraft:gold_block",
                    "Close lost the parked changes of a discarded load");
            reopened.closeStorage();
        });
        check("a synchronous load overtaking an async one keeps its parked changes", [&]() {
            Level overtaken("ParkedOvertake", 4, 1);
            require(overtaken.openStorage(WorldManager::getWorldsDirectory()), "Could not open overtake fixture");
            overtaken.startWorkers(1);
            LevelPersistenceProbe::stagePending(overtaken, 96, 90, 256, "minecraft:gold_block");
            overtaken.requestChunkAsync(96, 90);
            require(overtaken.getPendingChangeChunkCount() == 0, "Load did not take its parked changes");
            overtaken.getChunk(96, 90);
            LevelPersistenceProbe::finishWrites(overtaken);
            overtaken.drainCompletedChunks();
            require(overtaken.getBlockState(96 * 16, 140, 90 * 16).mName == "minecraft:gold_block",
                    "Rejected load result dropped its parked changes");
            overtaken.closeStorage();
        });
        check("raw terrain ignores blocks decorations left in the cache", [&]() {
            const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(
                    DimensionType::Overworld, 1, DimensionFactory::GeneratorType::Flat);
            TerrainCache cache(*generator);
            LevelChunk decorated = cache.neighbour(3, 3);
            decorated.setBlock(0, 140, 0, BlockState("minecraft:gold_block"));
            cache.put(std::move(decorated));
            require(cache.terrain(3, 3).getBlock(0, 140, 0).mName != "minecraft:gold_block",
                    "Raw terrain picked up a decorated neighbour");
            require(cache.neighbour(3, 3).getBlock(0, 140, 0).mName == "minecraft:gold_block",
                    "Decorated neighbour was not kept for the next decoration");
        });
        check("failed terrain generation releases its claim", [&]() {
            const ThrowOnceGenerator generator;
            TerrainCache cache(generator);
            bool thrown = false;
            try {
                cache.terrain(5, 5);
            } catch (const std::runtime_error &) {
                thrown = true;
            }
            require(thrown, "Generation failure was swallowed");
            // a leftover claim would make this wait forever
            cache.terrain(5, 5);
        });
        check("generation backlog survives close", [&]() {
            Level backlog("Backlog", 4, 1, DimensionType::Overworld);
            require(backlog.openStorage(WorldManager::getWorldsDirectory()), "Could not open fixture level");
            backlog.getChunk(0, 0);
            backlog.startWorkers(1);
            LevelPersistenceProbe::queueOverflow(backlog);
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
        check("an actor outside the loaded columns is saved where it stands", [&]() {
            const int64_t column = ((int64_t) 40 << 32) | (uint32_t) 40;
            require(handler.getWorldOf(level).getActorLoadedChunks(level.getDimensionType()).count(column) == 0,
                    "The fixture column already has its actors loaded");
            Tag stored = Tag::ofCompound();
            stored.putString("identifier", "minecraft:sheep");
            stored.putLong("UniqueID", 0x7fff00000001);
            level.saveEntities(40, 40, {stored});

            ServerActor *pig = handler.spawnActor(level, "minecraft:pig", Vector3f(648.5f, 100.0f, 648.5f));
            require(pig != nullptr, "Could not spawn the stray pig");
            const int64_t pigId = pig->getUniqueId();
            handler.saveAllActors();
            handler.saveAllActors();

            size_t pigs = 0;
            bool sheep = false;
            for (const Tag &entity: level.loadEntities(40, 40)) {
                pigs += entity.getLong("UniqueID", 0) == pigId ? 1 : 0;
                sheep = sheep || entity.getLong("UniqueID", 0) == 0x7fff00000001;
            }
            handler.removeActor(pigId);
            require(pigs == 1, "An actor outside the loaded columns was not saved once in its column");
            require(sheep, "Saving a stray actor dropped the actors stored in its column");
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
        check("packed sub-chunk storage reads back for every palette width", [&]() {
            const char *names[] = {
                    "minecraft:stone",   "minecraft:dirt",          "minecraft:cobblestone", "minecraft:sand",
                    "minecraft:gravel",  "minecraft:gold_ore",      "minecraft:iron_ore",    "minecraft:coal_ore",
                    "minecraft:glass",   "minecraft:diamond_block", "minecraft:gold_block",  "minecraft:iron_block",
                    "minecraft:bedrock", "minecraft:bookshelf",     "minecraft:obsidian",    "minecraft:ice",
                    "minecraft:clay",    "minecraft:netherrack",    "minecraft:soul_sand",   "minecraft:glowstone"};
            std::mt19937 random(11);
            for (size_t kinds = 1; kinds <= sizeof(names) / sizeof(names[0]); ++kinds) {
                SubChunk section(3);
                for (int index = 0; index < SubChunk::BLOCK_COUNT; ++index) {
                    if (random() % 4 != 0)
                        section.setBlock(index >> 8, index & 15, (index >> 4) & 15,
                                         BlockState(names[random() % kinds]));
                    section.setBiome(index >> 8, index & 15, (index >> 4) & 15, (uint32_t) (random() % kinds));
                }
                section.setBlock(1, 2, 3, 1, BlockState(names[kinds - 1]));

                BinaryStream persistent;
                section.writePersistent(persistent);
                section.writeBiomes(persistent, true);
                ReadOnlyBinaryStream input(persistent.getBuffer());
                SubChunk read(3);
                require(read.readPersistent(input) && read.readBiomes(input, nullptr), "Packed section did not parse");
                for (int index = 0; index < SubChunk::BLOCK_COUNT; ++index) {
                    const int x = index >> 8, y = index & 15, z = (index >> 4) & 15;
                    require(read.getBlock(x, y, z).getHash() == section.getBlock(x, y, z).getHash()
                                    && read.getBlock(x, y, z, 1).getHash() == section.getBlock(x, y, z, 1).getHash(),
                            "Packed section read back other blocks");
                    require(read.getBiome(x, y, z) == section.getBiome(x, y, z),
                            "Packed section read back other biomes");
                }

                BinaryStream network;
                section.writeNetwork(network);
                ReadOnlyBinaryStream packet(network.getBuffer());
                require(packet.getByte() == SubChunk::STORAGE_VERSION && packet.getByte() == SubChunk::LAYER_COUNT
                                && packet.getByte() == 3,
                        "Network section header changed");
                const int bits = packet.getByte() >> 1;
                const int perWord = 32 / bits;
                std::vector<uint32_t> indices;
                for (int word = 0; word < (SubChunk::BLOCK_COUNT + perWord - 1) / perWord; ++word) {
                    const uint32_t packed = packet.getLInt();
                    for (int slot = 0; slot < perWord && (int) indices.size() < SubChunk::BLOCK_COUNT; ++slot)
                        indices.push_back((packed >> (slot * bits)) & ((1u << bits) - 1u));
                }
                std::vector<int32_t> palette((size_t) packet.getVarInt());
                for (int32_t &entry: palette)
                    entry = packet.getVarInt();
                for (int index = 0; index < SubChunk::BLOCK_COUNT; ++index)
                    require(palette.at(indices[(size_t) index])
                                    == section.getBlock(index >> 8, index & 15, (index >> 4) & 15).getHash(),
                            "Network section indices point at other blocks");
            }
        });
        check("a packed chunk unpacks to exactly the same storage", [&]() {
            const char *names[] = {"minecraft:stone", "minecraft:dirt", "minecraft:water", "minecraft:glowstone",
                                   "minecraft:oak_log"};
            std::mt19937 random(3);
            LevelChunk chunk(2, -3);
            for (int i = 0; i < 20000; ++i) {
                const int x = (int) (random() % 16), z = (int) (random() % 16);
                const int32_t y = LevelChunk::MIN_Y + (int32_t) (random() % 200);
                chunk.setBlock(x, y, z, BlockState(names[random() % 5]));
                chunk.setBiomeAt(x, y, z, (uint32_t) (random() % 4));
                chunk.setSkyLight(x, y, z, (int) (random() % 16));
                chunk.setBlockLight(x, y, z, (int) (random() % 16));
            }
            chunk.setBlock(4, 10, 4, 1, BlockState("minecraft:water"));
            const auto persistent = [&chunk]() {
                BinaryStream stream;
                for (int index = 0; index < LevelChunk::SUB_CHUNK_COUNT; ++index)
                    chunk.getSubChunk(index).writePersistent(stream);
                return stream.getBuffer() + chunk.encodeBiomesPersistent(0, LevelChunk::SUB_CHUNK_COUNT);
            };
            const auto light = [&chunk]() {
                std::string levels;
                for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y; ++y) {
                    for (int x = 0; x < 16; ++x) {
                        levels += (char) chunk.getSkyLight(x, y, 15 - x);
                        levels += (char) chunk.getBlockLight(x, y, x);
                    }
                }
                return levels;
            };
            const std::string network = chunk.encodeNetwork();
            const std::string stored = persistent();
            const std::string levels = light();

            chunk.pack();
            require(chunk.isPacked(), "Chunk did not pack");
            require(chunk.encodeNetwork() == network && !chunk.isPacked(), "Unpacked chunk encodes differently");
            require(persistent() == stored && light() == levels, "Unpacked chunk lost storage");
            require(chunk.consumeAccessed() && !chunk.consumeAccessed(), "An access was not reported once");
            chunk.getBlock(1, 1, 1);
            require(chunk.consumeAccessed(), "A read of an unpacked chunk was not reported");

            chunk.pack();
            LevelChunk copy(chunk);
            copy.setBlock(1, 1, 1, BlockState("minecraft:dirt"));
            require(chunk.isPacked() && copy.getBlock(1, 1, 1).mName == "minecraft:dirt",
                    "A copy of a packed chunk did not unpack on its own");
        });
        check("light sections fold back to one level and release their table", [&]() {
            LightStorage light;
            light.set(0, 0, 0, 0);
            require(light.isAllocated() && light.get(3, 0, 3) == 0, "Uniform light allocated a section");
            light.set(1, 70, 2, 9);
            light.set(1, -60, 2, 4);
            require(light.get(1, 70, 2) == 9 && light.get(1, -60, 2) == 4 && light.get(2, 70, 2) == 0,
                    "Light levels were not kept");
            light.set(1, 70, 2, 0);
            light.compact();
            require(light.get(1, 70, 2) == 0 && light.get(1, -60, 2) == 4, "Compaction changed a light level");
            light.set(1, -60, 2, 0);
            light.compact();
            light.set(5, 5, 5, 0);
            require(light.get(1, -60, 2) == 0 && light.get(5, 5, 5) == 0, "Released sections kept a level");
        });
        check("inbound packets are read while a large outbound backlog compresses", [&]() {
            FakeConnector *connector = new FakeConnector();
            NetworkHandler network{std::unique_ptr<Connector>(connector)};
            RecordingListener listener;
            network.addListener(&listener);
            network.startIoThread();
            const NetworkIdentifier client("backlog", 1);
            std::shared_ptr<FakePeer> peer = std::make_shared<FakePeer>();
            connector->join(client, peer);
            pumpUntil(network, [&]() { return !listener.mEvents.empty(); }, "Connection never opened");
            network.enableCompression(client, CompressedNetworkPeer::CompressionAlgorithm::ZLib, 0);

            // random bytes keep deflate slow: the backlog outlasts one read pass
            std::mt19937 random(7);
            std::string payload(64 * 1024, '\0');
            for (char &byte: payload)
                byte = (char) random();
            const size_t packets = 1024;
            for (size_t index = 0; index < packets; ++index)
                network.send(client, payload);
            peer->deliver("ping", true);

            require(network.getPendingOutboundBytes(client) > 0, "Queued bytes were not counted");

            pumpUntil(network, [&]() { return listener.mEvents.back() == "data:ping"; }, "Inbound packet never read");
            require(peer->mSentBytes.load() < packets * payload.size() / 2,
                    "Inbound packet waited for the outbound backlog");
            pumpUntil(
                    network, [&]() { return network.getPendingOutboundBytes(client) == 0; },
                    "Queued bytes were never released");
            network.stopIoThread();
        });
        check("a reply is read in the state enabled right after the packet it answers", [&]() {
            FakeConnector *connector = new FakeConnector();
            NetworkHandler network{std::unique_ptr<Connector>(connector)};
            RecordingListener listener;
            network.addListener(&listener);
            const NetworkIdentifier client("settings", 1);
            std::shared_ptr<AnsweringPeer> peer = std::make_shared<AnsweringPeer>("pong");
            connector->join(client, peer);
            pumpUntil(network, [&]() { return !listener.mEvents.empty(); }, "Connection never opened");

            // fills the whole per-pass budget, queued before the shard threads start: one pass drains it
            // with the flush and compression switch after it
            network.send(client, std::string(256 * 1024, 'x'));
            network.flush(client);
            network.enableCompression(client, CompressedNetworkPeer::CompressionAlgorithm::ZLib, 0);
            network.startIoThread();
            pumpUntil(network, [&]() { return listener.mEvents.size() >= 2; }, "Reply never read");
            network.stopIoThread();
            require(listener.mEvents.back() == "data:pong", "Reply was read before compression was enabled");
        });
        check("a server close is reported once and stops the connection's data", [&]() {
            FakeConnector *connector = new FakeConnector();
            NetworkHandler network{std::unique_ptr<Connector>(connector)};
            RecordingListener listener;
            network.addListener(&listener);
            network.startIoThread();
            const NetworkIdentifier client("close", 1);
            std::shared_ptr<FakePeer> peer = std::make_shared<FakePeer>();
            connector->join(client, peer);
            peer->deliver("first");
            pumpUntil(network, [&]() { return listener.mEvents.size() >= 2; }, "First packet never read");
            network.closeConnection(client);
            pumpUntil(network, [&]() { return connector->closedCount() == 1; }, "Transport never closed");
            peer->deliver("late");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            network.runEvents();
            network.stopIoThread();
            const std::vector<std::string> expected = {"open", "data:first", "refused"};
            require(listener.mEvents == expected, "Close was not reported once after the connection's data");
            require(network.getConnectionCount() == 0, "Closed connection still counted");
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
