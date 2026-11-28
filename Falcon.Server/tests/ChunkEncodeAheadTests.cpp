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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
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

    using Plan = ChunkEncodeAhead;

    struct IntHasher {
        size_t operator()(int value) const {
            return std::hash<int>()(value);
        }
    };

    // players' next sends as keys: player p asks for first + p * step + i
    std::vector<std::vector<int>> picksOf(int players, int each, int step) {
        std::vector<std::vector<int>> picks(players);
        for (int p = 0; p < players; ++p)
            for (int i = 0; i < each; ++i)
                picks[p].push_back(p * step + i);
        return picks;
    }

    // a warm cache, no block actors: every stamp is a hit that keeps nothing and makes no job; the stamps (and the
    // storage reads they can make) still stop at the cap
    void cachedChunksWithoutBlockActorsAreBounded() {
        const auto picks = picksOf(50, 200, 1000);
        size_t stamps = 0, kept = 0;
        const Plan::PlanCounts counts = Plan::plan<int, IntHasher>(
                picks,
                [&](int) {
                    ++stamps;
                    return std::optional<Plan::Visit>(Plan::Visit{true, 0});
                },
                [&](int, const Plan::Visit &) { ++kept; });
        require(stamps == Plan::MAX_CHUNKS_PER_TICK, "cache hits were stamped past the cap: " + std::to_string(stamps));
        require(kept == Plan::MAX_CHUNKS_PER_TICK && counts.mChunks == Plan::MAX_CHUNKS_PER_TICK,
                "cache hits were kept past the cap: " + std::to_string(kept));
        require(counts.mJobs == 0 && counts.mBytes == 0, "cache hits made jobs or kept bytes");
    }

    // players next to each other ask for the same chunks: each is stamped once, hit or miss
    void sharedChunksAreStampedOnce() {
        const auto picks = picksOf(20, 100, 0); // all 20 players ask for keys 0..99
        std::vector<int> stamped(100, 0);
        for (bool cached: {true, false}) {
            std::fill(stamped.begin(), stamped.end(), 0);
            const Plan::PlanCounts counts = Plan::plan<int, IntHasher>(
                    picks,
                    [&](int key) {
                        ++stamped[key];
                        return std::optional<Plan::Visit>(Plan::Visit{cached, cached ? 0u : 100u});
                    },
                    [](int, const Plan::Visit &) {});
            for (int key = 0; key < 100; ++key)
                require(stamped[key] == 1, std::string(cached ? "a cache hit" : "a miss") + " was stamped "
                                                   + std::to_string(stamped[key]) + " times");
            require(counts.mChunks == 100 && counts.mJobs == (cached ? 0u : 100u), "shared chunks miscounted");
        }
    }

    // misses stop at the chunk cap; the round order gives every player their nearest chunks first
    void missesStopAtTheCapRoundByRound() {
        const auto picks = picksOf(4, 500, 1000);
        std::vector<int> keptOf(4, 0);
        const Plan::PlanCounts counts = Plan::plan<int, IntHasher>(
                picks, [](int) { return std::optional<Plan::Visit>(Plan::Visit{false, 1000}); },
                [&](int key, const Plan::Visit &) { ++keptOf[key / 1000]; });
        require(counts.mJobs == Plan::MAX_CHUNKS_PER_TICK, "misses past the cap became jobs");
        for (int p = 0; p < 4; ++p)
            require(keptOf[p] == (int) Plan::MAX_CHUNKS_PER_TICK / 4, "player " + std::to_string(p) + " got "
                                                                          + std::to_string(keptOf[p]) + " chunks");
    }

    // chunks that are not loaded take no stamp and leave the cap to the ones that are
    void unloadedChunksTakeNoStamp() {
        const auto picks = picksOf(1, 1000, 0);
        const Plan::PlanCounts counts = Plan::plan<int, IntHasher>(
                picks,
                [](int key) {
                    if (key % 2 == 0)
                        return std::optional<Plan::Visit>();
                    return std::optional<Plan::Visit>(Plan::Visit{true, 0});
                },
                [](int key, const Plan::Visit &) { require(key % 2 == 1, "an unloaded chunk was kept"); });
        require(counts.mChunks == Plan::MAX_CHUNKS_PER_TICK, "unloaded chunks used up the cap");
    }

    // the byte cap stops the batch before the chunk that would pass it is kept
    void bytesStopBeforeTheChunkThatPassesTheCap() {
        const auto picks = picksOf(1, 100, 0);
        const size_t each = Plan::MAX_COPIED_BYTES_PER_TICK / 10 + 1;
        size_t kept = 0;
        const Plan::PlanCounts counts = Plan::plan<int, IntHasher>(
                picks, [&](int) { return std::optional<Plan::Visit>(Plan::Visit{false, each}); },
                [&](int, const Plan::Visit &) { ++kept; });
        require(kept == 9 && counts.mBytes == 9 * each && counts.mBytes <= Plan::MAX_COPIED_BYTES_PER_TICK,
                "the byte cap kept " + std::to_string(kept) + " chunks");
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
    check("cached chunks without block actors stop at the chunk cap", cachedChunksWithoutBlockActorsAreBounded);
    check("chunks several players ask for are stamped once", sharedChunksAreStampedOnce);
    check("misses stop at the chunk cap, round by round", missesStopAtTheCapRoundByRound);
    check("chunks not loaded take no stamp", unloadedChunksTakeNoStamp);
    check("the byte cap stops before the chunk that passes it", bytesStopBeforeTheChunkThatPassesTheCap);

    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
