#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/ChunkWorker.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Chunk/SubChunk.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"
#include "World/Level/Storage/LevelStorage.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
    const int64_t SEED = 42;
    const int32_t AREA_X = -2;
    const int32_t AREA_Z = -2;
    const int32_t AREA_SIZE = 5;

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    uint64_t mix(uint64_t hash, uint64_t value) {
        return hash ^ (value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2));
    }

    using Order = std::vector<std::pair<int32_t, int32_t>>;

    Order areaOrder() {
        Order order;
        for (int32_t chunkZ = AREA_Z; chunkZ < AREA_Z + AREA_SIZE; chunkZ++) {
            for (int32_t chunkX = AREA_X; chunkX < AREA_X + AREA_SIZE; chunkX++)
                order.emplace_back(chunkX, chunkZ);
        }
        return order;
    }

    // every block, block actor and entity of each chunk of the area, hashed in area order
    struct Area {
        std::vector<uint64_t> mHashes = std::vector<uint64_t>((size_t) (AREA_SIZE * AREA_SIZE), 0);
        size_t mBlockActors = 0;

        void add(const LevelChunk &chunk, const std::vector<Tag> &blockActors, const std::vector<Tag> &entities) {
            uint64_t hash = 0;
            for (int x = 0; x < 16; x++) {
                for (int z = 0; z < 16; z++) {
                    for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y; y++) {
                        hash = mix(hash, (uint32_t) chunk.getBlock(x, y, z, 0).getHash());
                        hash = mix(hash, (uint32_t) chunk.getBlock(x, y, z, 1).getHash());
                    }
                }
            }
            for (const Tag &actor: blockActors) {
                hash = mix(hash, (uint32_t) actor.getInt("x"));
                hash = mix(hash, (uint32_t) actor.getInt("y"));
                hash = mix(hash, (uint32_t) actor.getInt("z"));
            }
            hash = mix(hash, entities.size());
            mBlockActors += blockActors.size();
            const size_t index = (size_t) ((chunk.getZ() - AREA_Z) * AREA_SIZE + chunk.getX() - AREA_X);
            mHashes[index] = hash;
        }
    };

    Area generate(const Order &order, size_t threads) {
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                         SEED);
        TerrainCache cache(*generator);
        Area area;
        std::mutex mutex;
        std::atomic<size_t> next(0);
        std::vector<std::thread> workers;
        // built on this thread: a generator's constructor sets shared noise, one at a time
        std::vector<std::unique_ptr<GeneratorChunkSource>> sources;
        for (size_t i = 0; i < threads; i++)
            sources.push_back(std::unique_ptr<GeneratorChunkSource>(new GeneratorChunkSource(SEED, DimensionType::Overworld, cache)));
        for (size_t i = 0; i < threads; i++) {
            workers.emplace_back([&, i]() {
                GeneratorChunkSource &source = *sources[i];
                for (size_t index = next++; index < order.size(); index = next++) {
                    std::vector<Tag> blockActors;
                    std::vector<Tag> entities;
                    const LevelChunk chunk = source.generate(order[index].first, order[index].second, blockActors,
                                                             entities);
                    std::lock_guard<std::mutex> lock(mutex);
                    area.add(chunk, blockActors, entities);
                }
            });
        }
        for (std::thread &worker: workers)
            worker.join();
        return area;
    }

    void classOrder() {
        require(TerrainCache::decorationClass(0, 0) == 0, "z even, x even is not first");
        require(TerrainCache::decorationClass(1, 0) == 1, "z even, x odd is not second");
        require(TerrainCache::decorationClass(1, 1) == 2, "z odd, x odd is not third");
        require(TerrainCache::decorationClass(0, 1) == 3, "z odd, x even is not last");
        require(TerrainCache::decorationClass(-2, -4) == 0, "negative even chunks are not first");
        require(TerrainCache::decorationClass(-1, -3) == 2, "negative odd chunks are not third");
        // any two chunks side by side differ: the order between them is total
        for (int32_t chunkX = -3; chunkX <= 3; chunkX++) {
            for (int32_t chunkZ = -3; chunkZ <= 3; chunkZ++) {
                for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
                    for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
                        if (offsetX == 0 && offsetZ == 0)
                            continue;
                        require(TerrainCache::decorationClass(chunkX, chunkZ)
                                        != TerrainCache::decorationClass(chunkX + offsetX, chunkZ + offsetZ),
                                "two neighbours share a class");
                    }
                }
            }
        }
    }

    // a write lands only on the block its decoration saw
    void writesNeedTheBlockSeen() {
        LevelChunk chunk(0, 0);
        const BlockState *air = &chunk.getBlock(1, 70, 1);
        const BlockState *gold = SubChunk::intern(BlockState("minecraft:gold_block"));
        const BlockState *iron = SubChunk::intern(BlockState("minecraft:iron_block"));
        const uint32_t index = (uint32_t) (1 | 1 << 4 | (70 - LevelChunk::MIN_Y) << 8);
        ChunkDecoration first;
        first.add(4, {index, air, gold});
        GeneratorChunkSource::apply(chunk, first, 4);
        require(&chunk.getBlock(1, 70, 1) == gold, "a write over the block it saw was dropped");
        ChunkDecoration second;
        second.add(4, {index, air, iron});
        GeneratorChunkSource::apply(chunk, second, 4);
        require(&chunk.getBlock(1, 70, 1) == gold, "a write over another block than the one it saw was placed");

        std::vector<ChunkWrite> log;
        chunk.setWriteLog(&log);
        chunk.setBlock(2, 70, 2, *iron);
        chunk.setWriteLog(nullptr);
        chunk.setBlock(3, 70, 3, *iron);
        require(log.size() == 1 && log[0].mBefore == air && log[0].mAfter == iron, "the write log missed a block");
        LevelChunk copy = chunk;
        copy.setBlock(4, 70, 4, *iron);
        require(log.size() == 1, "a copy of a logged chunk logged its writes");
    }

    // the same chunks asked for in other orders and on several threads: the same blocks, actors and entities
    void ordersAgree() {
        const Order rows = areaOrder();
        const Area reference = generate(rows, 1);

        Order reversed = rows;
        std::reverse(reversed.begin(), reversed.end());
        Order shuffled = rows;
        std::mt19937 random(7);
        std::shuffle(shuffled.begin(), shuffled.end(), random);
        // spiral from the centre
        Order spiral = rows;
        std::stable_sort(spiral.begin(), spiral.end(), [](const auto &a, const auto &b) {
            const int32_t centre = AREA_SIZE / 2;
            return std::max(std::abs(a.first - AREA_X - centre), std::abs(a.second - AREA_Z - centre))
                   < std::max(std::abs(b.first - AREA_X - centre), std::abs(b.second - AREA_Z - centre));
        });

        require(generate(reversed, 1).mHashes == reference.mHashes, "the reversed order changed the area");
        require(generate(shuffled, 1).mHashes == reference.mHashes, "a random order changed the area");
        require(generate(spiral, 4).mHashes == reference.mHashes, "four threads changed the area");
    }

    // a decoration's blocks in a neighbour reach that neighbour, unless a decoration after it wrote there
    void writesReachNeighbours() {
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                         SEED);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(SEED, DimensionType::Overworld, cache);
        size_t reached = 0;
        for (int32_t chunkX = 0; chunkX < 4 && reached == 0; chunkX++) {
            const std::shared_ptr<const ChunkDecoration> decoration = source.decoration(chunkX, 0);
            // slot of the chunk east of it
            const size_t east = (size_t) ((1 + 1) * 3 + 1);
            if (decoration->mWrites[east].empty())
                continue;

            std::vector<Tag> blockActors;
            std::vector<Tag> entities;
            const LevelChunk neighbour = source.generate(chunkX + 1, 0, blockActors, entities);
            for (size_t index = 0; index < decoration->mWrites[east].size(); index++) {
                const ChunkWrite write = decoration->get(east, index);
                const int x = (int) (write.mIndex & 15);
                const int z = (int) ((write.mIndex >> 4) & 15);
                const int32_t y = (int32_t) ((write.mIndex >> 8) & 511) + LevelChunk::MIN_Y;
                if (&neighbour.getBlock(x, y, z, (int) (write.mIndex >> 17)) == write.mAfter)
                    reached++;
            }
        }
        require(reached != 0, "no decoration reached its neighbour");
    }
}

namespace {
    uint64_t hashBlocks(const LevelChunk &chunk) {
        uint64_t hash = 0;
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y; y++) {
                    hash = mix(hash, (uint32_t) chunk.getBlock(x, y, z, 0).getHash());
                    hash = mix(hash, (uint32_t) chunk.getBlock(x, y, z, 1).getHash());
                }
            }
        }
        return hash;
    }

    ChunkLoadResult awaitResult(ChunkWorker &worker) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
        while (std::chrono::steady_clock::now() < deadline) {
            std::vector<ChunkLoadResult> results = worker.drainCompleted();
            if (!results.empty())
                return std::move(results.front());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        throw std::runtime_error("the worker never finished the chunk");
    }

    GeneratedBlockChange goldAt(int32_t chunkX, int32_t chunkZ) {
        GeneratedBlockChange change;
        change.mX = chunkX * 16 + 3;
        change.mY = 300;
        change.mZ = chunkZ * 16 + 5;
        change.mState = SubChunk::intern(BlockState("minecraft:gold_block"));
        return change;
    }

    void workersFinishChunks() {
        const std::filesystem::path worlds = std::filesystem::temp_directory_path()
                                             / ("falcon-decoration-order-"
                                                + std::to_string(std::chrono::steady_clock::now()
                                                                         .time_since_epoch()
                                                                         .count()));
        std::filesystem::create_directories(worlds);
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                         SEED);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(SEED, DimensionType::Overworld, cache);
        std::vector<Tag> blockActors;
        std::vector<Tag> entities;
        {
            LevelStorage storage;
            require(storage.open(worlds.string(), "worker", 0), "the fixture world did not open");
            ChunkWorker worker(*generator, storage);
            worker.start(2);

            std::vector<GeneratedBlockChange> parked{goldAt(0, 0)};
            require(worker.requestLoad(0, 0, parked), "the load was refused");
            ChunkLoadResult fresh = awaitResult(worker);
            LevelChunk expected = source.generate(0, 0, blockActors, entities);
            expected.setBlock(3, 300, 5, *goldAt(0, 0).mState);
            require(fresh.mChunk != nullptr && fresh.mGenerated, "the worker returned no generated chunk");
            require(hashBlocks(*fresh.mChunk) == hashBlocks(expected),
                    "the worker's chunk is not the source's chunk with its parked block");

            // a neighbour whose decoration writes into the chunk west of it
            int32_t target = -1;
            for (int32_t chunkX = 4; chunkX < 12 && target < 0; chunkX++) {
                if (!source.decoration(chunkX + 1, 3)->mWrites[(size_t) ((-1 + 1) * 3 + 1)].empty())
                    target = chunkX;
            }
            require(target >= 0, "no decoration writes into its west neighbour");
            require(storage.saveChunk(source.generate(target + 1, 3, blockActors, entities)),
                    "the stored neighbour was not saved");
            require(storage.mergePendingBlockChanges(target, 3, {goldAt(target, 3)}), "the spill was not parked");

            std::vector<GeneratedBlockChange> none;
            require(worker.requestLoad(target, 3, none), "the second load was refused");
            ChunkLoadResult old = awaitResult(worker);
            const uint16_t east = (uint16_t) (1u << ((1 + 1) * 3 + 1));
            LevelChunk withNeighbour = source.generate(target, 3, blockActors, entities);
            require(hashBlocks(source.generate(target, 3, blockActors, entities, east)) != hashBlocks(withNeighbour),
                    "the saved neighbour placed nothing in the chunk");
            withNeighbour.setBlock(3, 300, 5, *goldAt(target, 3).mState);
            require(hashBlocks(*old.mChunk) == hashBlocks(withNeighbour),
                    "the parked block suppressed the saved neighbour's decoration");
            require(storage.loadPendingBlockChanges(target, 3).empty(), "the applied spill stayed in storage");
            worker.stop();
        }
        std::filesystem::remove_all(worlds);
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    BlockPaletteRegistry::getInstance().initialize();
    BiomeChunkGenDataRegistry::initialize();

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

    check("decoration classes order every pair of neighbours", classOrder);
    check("a decoration's write needs the block it saw", writesNeedTheBlockSeen);
    check("decorations write into their neighbours", writesReachNeighbours);
    check("request orders and threads give the same chunks", ordersAgree);
    check("workers finish chunks with their parked blocks", workersFinishChunks);

    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
