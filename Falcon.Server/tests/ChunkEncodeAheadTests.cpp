#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Network/ChunkEncodeAhead.h"
#include "Network/ChunkPacketCache.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/BlockDefinitionRegistry.h"
#include "Protocol/Types/BlockNetworkIdMap.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // block states from the vanilla palette, enough for every network palette width
    const std::vector<BlockState> &states() {
        static const std::vector<BlockState> all = []() {
            std::vector<BlockState> found;
            const BlockPaletteRegistry &palette = BlockPaletteRegistry::getInstance();
            for (const std::string &name: palette.getBlockNames()) {
                const std::vector<Tag> *permutations = palette.getPermutations(name);
                if (permutations == nullptr)
                    continue;
                for (const Tag &permutation: *permutations) {
                    found.emplace_back(name, permutation);
                    if (found.size() >= 400)
                        return found;
                }
            }
            return found;
        }();
        return all;
    }

    struct Shape {
        size_t mDistinctStates;
        int mLowestY;
        int mHighestY;
        bool mSecondLayer;
        bool mMixedBiomes;
    };

    void fill(LevelChunk &chunk, const Shape &shape, uint32_t seed) {
        std::mt19937 random(seed);
        const std::vector<BlockState> &pool = states();
        require(pool.size() >= shape.mDistinctStates, "too few vanilla states for the test");
        std::uniform_int_distribution<size_t> pick(0, shape.mDistinctStates - 1);
        const BlockState water = VanillaBlocks::WATER().toBlockState();
        for (int32_t y = shape.mLowestY; y <= shape.mHighestY; ++y) {
            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    chunk.setBlock(x, y, z, pool[pick(random)]);
                    if (shape.mSecondLayer && random() % 7 == 0)
                        chunk.setBlock(x, y, z, 1, water);
                    if (shape.mMixedBiomes && random() % 5 == 0)
                        chunk.setBiomeAt(x, y, z, (uint32_t) (random() % 40));
                }
            }
        }
    }

    std::vector<Shape> shapes() {
        std::vector<Shape> all;
        for (const size_t distinct: {1u, 2u, 3u, 5u, 9u, 17u, 33u, 65u, 129u, 300u})
            all.push_back(Shape{distinct, 0, 15, false, false});
        all.push_back(Shape{40, -64, 319, true, true});
        all.push_back(Shape{300, -64, -60, true, false});
        all.push_back(Shape{6, 300, 319, false, true});
        all.push_back(Shape{20, 60, 90, true, true});
        return all;
    }

    // what the main thread sends: LevelChunk::encodeNetwork through Level::getChunkData
    std::string encodedOnMain(LevelChunk &chunk, const BlockNetworkIdMap *blockIds) {
        std::string data = chunk.encodeNetwork(blockIds);
        chunk.invalidateNetworkCaches();
        return data;
    }

    std::shared_ptr<BlockNetworkIdMap> translation() {
        auto map = std::make_shared<BlockNetworkIdMap>();
        const std::vector<BlockState> &pool = states();
        for (size_t i = 0; i + 1 < pool.size(); i += 3)
            map->add(pool[i].getHash(), pool[i + 1].getHash());
        return map;
    }

    // the batch cap is checked before copying: the size it reads must be the copy's
    void copySizeKnownBeforeCopy() {
        uint32_t seed = 500;
        for (const Shape &shape: shapes()) {
            LevelChunk chunk(2, 2);
            fill(chunk, shape, seed++);
            const size_t before = chunk.snapshotNetworkBytes();
            require(before == chunk.snapshotNetwork().copiedBytes(),
                    "the size read before copying differs from the copy with " + std::to_string(shape.mDistinctStates)
                            + " states");
            require(before > 0, "a filled chunk reports an empty copy");
        }
        LevelChunk empty(0, 0);
        require(empty.snapshotNetworkBytes() == 0 && empty.snapshotNetwork().copiedBytes() == 0,
                "an empty chunk reports a copy");
    }

    // the batch cap counts the block actors with the copy, and both places they are kept, before keeping anything
    void retainedBytesCountBlockActors() {
        const size_t copy = 70000;
        const size_t actors = 4096;
        require(ChunkEncodeAhead::retainedBytes(false, false, copy, actors) == copy + actors,
                "a job's block actors are left out of the count");
        require(ChunkEncodeAhead::retainedBytes(true, false, copy, actors) == copy + actors + actors,
                "block actors kept for the send and in the job are counted once");
        require(ChunkEncodeAhead::retainedBytes(true, true, copy, actors) == actors,
                "a cached chunk's block actors kept for its send are not counted");
        require(ChunkEncodeAhead::retainedBytes(false, true, copy, actors) == 0,
                "a cached chunk whose block actors are loaded counts bytes it does not keep");

        // a chunk whose copy alone fits but whose block actors push past the cap stays out of the batch
        const size_t used = ChunkEncodeAhead::MAX_COPIED_BYTES_PER_TICK - copy;
        require(used + copy <= ChunkEncodeAhead::MAX_COPIED_BYTES_PER_TICK
                        && used + ChunkEncodeAhead::retainedBytes(false, false, copy, actors)
                                   > ChunkEncodeAhead::MAX_COPIED_BYTES_PER_TICK,
                "block actors past the cap are not caught before the chunk is kept");
    }

    void emptyChunk() {
        LevelChunk chunk(3, -7);
        require(chunk.snapshotNetwork().encode(nullptr) == encodedOnMain(chunk, nullptr),
                "an empty chunk encodes differently from its copy");
    }

    void sectionsMatch() {
        const std::shared_ptr<BlockNetworkIdMap> map = translation();
        uint32_t seed = 1;
        for (const Shape &shape: shapes()) {
            LevelChunk chunk(5, 9);
            fill(chunk, shape, seed++);
            const LevelChunkNetworkSnapshot snapshot = chunk.snapshotNetwork();
            require(snapshot.encode(nullptr) == encodedOnMain(chunk, nullptr),
                    "copy differs from the chunk with " + std::to_string(shape.mDistinctStates) + " states");
            require(snapshot.encode(map.get()) == encodedOnMain(chunk, map.get()),
                    "translated copy differs with " + std::to_string(shape.mDistinctStates) + " states");
            require(snapshot.mSections.size() == (size_t) chunk.getNetworkSubChunkCount(),
                    "the copy holds another section count than the packet header");
        }
    }

    void packetsMatch() {
        BlockDefinitionRegistry blocks;
        ItemDefinitionRegistry items;
        PacketCodecContext current(blocks, items);
        PacketCodecContext older(blocks, items);
        older.setBlockNetworkIds(translation());
        const std::string blockActors("\x0a\x00\x00\x03\x01\x02\x03", 7);

        uint32_t seed = 100;
        for (const PacketCodecContext *context: {&current, &older}) {
            for (const Shape &shape: shapes()) {
                LevelChunk chunk(-12, 31);
                fill(chunk, shape, seed++);
                ChunkPacketCache::Stamp stamp;
                stamp.mBlockActors = blockActors;
                const ChunkEncodeAhead::Job job{ChunkPacketCache::Key{nullptr, 0, context}, stamp, *context,
                                                chunk.snapshotNetwork(), -12, 31, 1, std::string()};

                const uint32_t subChunks = (uint32_t) chunk.getNetworkSubChunkCount();
                const std::string onMain = ChunkStreamHandler::writeChunkPacket(
                        -12, 31, 1, subChunks, encodedOnMain(chunk, context->getBlockNetworkIds()) + blockActors,
                        *context);
                require(ChunkEncodeAhead::encode(job) == onMain,
                        "a worker's packet differs from the main thread's with "
                                + std::to_string(shape.mDistinctStates) + " states");
            }
        }
    }

    // workers read only their copies: the main thread editing the chunks meanwhile changes no packet
    void copiesAreImmutable() {
        BlockDefinitionRegistry blocks;
        ItemDefinitionRegistry items;
        PacketCodecContext context(blocks, items);

        std::vector<LevelChunk> chunks;
        std::vector<ChunkEncodeAhead::Job> jobs;
        std::vector<std::string> expected;
        const std::vector<Shape> all = shapes();
        for (size_t i = 0; i < 64; ++i) {
            chunks.emplace_back((int32_t) i, 0);
            fill(chunks.back(), all[i % all.size()], (uint32_t) (1000 + i));
        }
        for (size_t i = 0; i < chunks.size(); ++i) {
            ChunkPacketCache::Stamp stamp;
            jobs.push_back(ChunkEncodeAhead::Job{ChunkPacketCache::Key{nullptr, (int64_t) i, &context}, stamp,
                                                 context, chunks[i].snapshotNetwork(), (int32_t) i, 0, 0,
                                                 std::string()});
            expected.push_back(ChunkEncodeAhead::encode(jobs.back()));
        }

        std::atomic<size_t> next{0};
        std::vector<std::thread> workers;
        for (int t = 0; t < 4; ++t) {
            workers.emplace_back([&]() {
                for (size_t index = next++; index < jobs.size(); index = next++)
                    jobs[index].mEncoded = ChunkEncodeAhead::encode(jobs[index]);
            });
        }
        std::mt19937 random(7);
        const std::vector<BlockState> &pool = states();
        for (int edit = 0; edit < 20000; ++edit) {
            LevelChunk &chunk = chunks[random() % chunks.size()];
            chunk.setBlock((int) (random() % 16), (int32_t) (random() % 384) - 64, (int) (random() % 16),
                           pool[random() % pool.size()]);
        }
        for (std::thread &worker: workers)
            worker.join();

        for (size_t i = 0; i < jobs.size(); ++i)
            require(jobs[i].mEncoded == expected[i], "an encoding changed with its source chunk");
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

    check("empty chunk copy encodes like the chunk", emptyChunk);
    check("copy size is known before the copy", copySizeKnownBeforeCopy);
    check("the batch cap counts block actors before keeping them", retainedBytesCountBlockActors);
    check("chunk copy encodes like the chunk, every palette width", sectionsMatch);
    check("worker packet equals the main thread packet", packetsMatch);
    check("copies stay the same while the chunks change", copiesAreImmutable);

    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
