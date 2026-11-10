#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Network/ChunkPacketCache.h"
#include "Network/Handler/NetworkHandler.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ChunkPacketCache::Stamp stampOf(const LevelChunk &chunk, const std::string &blockActors = std::string()) {
        ChunkPacketCache::Stamp stamp;
        stamp.mInstance = chunk.getNetworkInstance();
        stamp.mRevision = chunk.getNetworkRevision();
        stamp.mBlockActors = blockActors;
        return stamp;
    }

    void chunkRevision() {
        LevelChunk chunk(0, 0);
        const uint32_t initial = chunk.getNetworkRevision();

        const LevelChunk &view = chunk;
        (void) view.getBlock(1, 64, 1);
        (void) view.getSubChunk(8);
        require(chunk.getNetworkRevision() == initial, "a read changes the revision");

        chunk.setBlock(1, 64, 1, VanillaBlocks::STONE().toBlockState());
        require(chunk.getNetworkRevision() != initial, "a block edit keeps the revision");

        const uint32_t edited = chunk.getNetworkRevision();
        chunk.setBiomeAt(1, 64, 1, 2);
        require(chunk.getNetworkRevision() != edited, "a biome edit keeps the revision");

        const LevelChunk copy(chunk);
        require(copy.getNetworkInstance() != chunk.getNetworkInstance(), "a copy shares its source instance");

        LevelChunk assigned(1, 1);
        const uint64_t before = assigned.getNetworkInstance();
        assigned = chunk;
        require(assigned.getNetworkInstance() != before && assigned.getNetworkInstance() != chunk.getNetworkInstance(),
                "an assigned chunk keeps an instance");
    }

    void cacheSharesEncoding() {
        LevelChunk chunk(0, 0);
        ChunkPacketCache cache;
        int encodes = 0;
        const auto encode = [&encodes]() {
            ++encodes;
            return std::string("chunk");
        };
        const ChunkPacketCache::Key key{&cache, 7, nullptr};

        const std::shared_ptr<const SharedPacket> shared = cache.get(key, stampOf(chunk), 1, encode);
        require(shared != nullptr && encodes == 1, "a first send is not encoded");
        require(shared->getPacket() == "chunk", "the shared packet lost its bytes");
        require(cache.get(key, stampOf(chunk), 2, encode) == shared && encodes == 1, "a second send encodes again");

        const ChunkPacketCache::Key otherContext{&cache, 7, &encodes};
        require(cache.get(otherContext, stampOf(chunk), 3, encode) != shared && encodes == 2,
                "another codec context reuses bytes");
    }

    void cacheDropsStaleRevision() {
        LevelChunk chunk(0, 0);
        ChunkPacketCache cache;
        const auto encode = []() { return std::string("chunk"); };
        const ChunkPacketCache::Key key{&cache, 7, nullptr};

        const std::shared_ptr<const SharedPacket> first = cache.get(key, stampOf(chunk), 1, encode);

        chunk.setBlock(0, 64, 0, VanillaBlocks::STONE().toBlockState());
        const std::shared_ptr<const SharedPacket> edited = cache.get(key, stampOf(chunk), 2, encode);
        require(edited != first, "an edited chunk reuses old bytes");

        const std::shared_ptr<const SharedPacket> withSign = cache.get(key, stampOf(chunk, "sign"), 3, encode);
        require(withSign != edited, "new block actors reuse old bytes");

        const LevelChunk reloaded(chunk);
        require(cache.get(key, stampOf(reloaded, "sign"), 4, encode) != withSign, "a reloaded chunk reuses old bytes");
    }

    void cacheExpires() {
        LevelChunk chunk(0, 0);
        ChunkPacketCache cache;
        const auto encode = []() { return std::string("chunk"); };
        cache.get({&cache, 1, nullptr}, stampOf(chunk), 10, encode);
        cache.get({&cache, 2, nullptr}, stampOf(chunk), 10 + ChunkPacketCache::EXPIRY_TICKS, encode);

        cache.expire(10 + ChunkPacketCache::EXPIRY_TICKS);
        require(cache.size() == 2, "an entry expired early");

        cache.expire(11 + ChunkPacketCache::EXPIRY_TICKS);
        require(cache.size() == 1, "an unused entry outlived its expiry");
    }

    class QueuedPeer : public NetworkPeer {
    public:
        explicit QueuedPeer(std::vector<std::string> packets) {
            for (const std::string &packet: packets) {
                std::string batch(1, (char) packet.size());
                mIncoming.push_back(batch + packet);
            }
        }

        void sendPacket(const std::string &, Reliability, Compressibility) override {}

        DataStatus receivePacket(std::string &outData) override {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mIncoming.empty())
                return DataStatus::NoData;

            outData = mIncoming.front();
            mIncoming.pop_front();
            return DataStatus::HasData;
        }

        NetworkStatus getNetworkStatus() const override {
            return NetworkStatus();
        }

        bool usesGamePacketId() const override {
            return false;
        }

    private:
        std::mutex mMutex;
        std::deque<std::string> mIncoming;
    };

    class AnnouncingConnector : public Connector {
    public:
        AnnouncingConnector(NetworkIdentifier first, NetworkIdentifier second)
                : mFirst(std::move(first)), mSecond(std::move(second)) {}

        bool host(const ConnectionDefinition &) override {
            return true;
        }

        void disconnect() override {}

        void runEvents() override {
            if (mAnnounced || mCallbacks == nullptr)
                return;

            mAnnounced = true;
            mCallbacks->onNewIncomingConnection(
                    mFirst, std::make_shared<QueuedPeer>(std::vector<std::string>{"login", "handshake"}));
            mCallbacks->onNewIncomingConnection(mSecond,
                                                std::make_shared<QueuedPeer>(std::vector<std::string>{"move"}));
        }

        std::shared_ptr<NetworkPeer> getPeerForUser(const NetworkIdentifier &) override {
            return nullptr;
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

    private:
        NetworkIdentifier mFirst;
        NetworkIdentifier mSecond;
        bool mAnnounced = false;
    };

    class RecordingListener : public NetworkHandler::Listener {
    public:
        void onNewIncomingConnection(const NetworkIdentifier &id) override {
            mEvents.push_back(id.toString() + " open");
        }

        void onDataReceived(const NetworkIdentifier &id, const std::string &data) override {
            mEvents.push_back(id.toString() + " " + data);
        }

        std::vector<std::string> mEvents;
    };

    void loginEventsRunFirst() {
        const NetworkIdentifier joining("1", 1);
        const NetworkIdentifier playing("2", 2);
        NetworkHandler network(std::unique_ptr<Connector>(new AnnouncingConnector(joining, playing)));
        RecordingListener listener;
        network.addListener(&listener);
        network.startIoThread();

        const auto admit = [&joining](const NetworkIdentifier &id) { return id == joining; };
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (listener.mEvents.size() < 3 && std::chrono::steady_clock::now() < deadline) {
            network.runEventsFor(admit);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        network.runEventsFor(admit);

        const std::vector<std::string> early = {joining.toString() + " open", joining.toString() + " login",
                                                joining.toString() + " handshake"};
        require(listener.mEvents == early, "a held connection ran between ticks");

        network.runEvents();
        std::vector<std::string> all = early;
        all.push_back(playing.toString() + " open");
        all.push_back(playing.toString() + " move");
        require(listener.mEvents == all, "held events lost their order");

        network.stopIoThread();
        network.removeListener(&listener);
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    BlockPaletteRegistry::getInstance().initialize();

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

    check("chunk network revision", chunkRevision);
    check("chunk packet encoded once", cacheSharesEncoding);
    check("chunk packet dropped on a new revision", cacheDropsStaleRevision);
    check("chunk packet expiry", cacheExpires);
    check("login events run between ticks in order", loginEventsRunFirst);

    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
