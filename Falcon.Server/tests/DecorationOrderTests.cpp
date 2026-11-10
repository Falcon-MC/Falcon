#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::unique_ptr<ChunkGenerator> flatGenerator() {
        return DimensionFactory::createGenerator(DimensionType::Overworld, 1, DimensionFactory::GeneratorType::Flat);
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

    void holdsRule() {
        require(decorationHolds(4, 9, 5), "a decoration between base and own was applied twice");
        require(!decorationHolds(4, 9, 4), "a decoration from before the base was dropped");
        require(!decorationHolds(4, 9, 12), "a later decoration was dropped");
        require(!decorationHolds(4, 9, 0), "a change of unknown order was dropped");
        require(!decorationHolds(0, 0, 3), "a chunk decorated in an earlier session dropped a change");
    }

    void neighboursWait() {
        const std::unique_ptr<ChunkGenerator> generator = flatGenerator();
        TerrainCache cache(*generator);
        cache.expectDecoration(0, 0);
        require(cache.awaitsEarlierNeighbour(1, 1), "a later class did not wait for its earlier neighbour");
        require(cache.awaitsEarlierNeighbour(-1, -1), "a diagonal later class did not wait");
        require(!cache.awaitsEarlierNeighbour(2, 2), "a chunk waited for one two chunks away");
        cache.expectDecoration(1, 1);
        require(!cache.awaitsEarlierNeighbour(0, 0), "the first class waited for a later one");
        cache.finishDecoration(0, 0);
        require(!cache.awaitsEarlierNeighbour(1, 1), "a finished decoration still held its neighbour");
    }

    void areaLock() {
        const std::unique_ptr<ChunkGenerator> generator = flatGenerator();
        TerrainCache cache(*generator);
        std::unique_ptr<TerrainCache::DecorationArea> first = cache.lockArea(0, 0);
        const std::unique_ptr<TerrainCache::DecorationArea> apart = cache.lockArea(4, 0);
        require(apart->getSequence() > first->getSequence(), "sequences do not follow the locks");

        std::atomic<bool> locked(false);
        uint64_t sequence = 0;
        std::thread neighbour([&]() {
            const std::unique_ptr<TerrainCache::DecorationArea> area = cache.lockArea(1, 1);
            sequence = area->getSequence();
            locked.store(true);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        require(!locked.load(), "two neighbours decorated at once");
        first.reset();
        neighbour.join();
        require(locked.load() && sequence > apart->getSequence(), "the neighbour never got the area");
    }

    void baseSequence() {
        const std::unique_ptr<ChunkGenerator> generator = flatGenerator();
        TerrainCache cache(*generator);
        {
            const std::unique_ptr<TerrainCache::DecorationArea> area = cache.lockArea(10, 10);
        }
        uint64_t created = 99;
        cache.base(0, 0, created);
        require(created == 1, "a new entry does not record the decorations before it");

        LevelChunk decorated = cache.neighbour(0, 0);
        decorated.setBlock(0, 140, 0, BlockState("minecraft:gold_block"));
        cache.put(std::move(decorated));
        require(cache.base(0, 0, created).getBlock(0, 140, 0).mName == "minecraft:gold_block",
                "a generated chunk ignored what its neighbours spilled into the cache");
        require(created == 1, "a put moved the entry's creation");
        require(cache.terrain(0, 0).getBlock(0, 140, 0).mName != "minecraft:gold_block",
                "raw terrain picked up a decoration");
    }

    // a neighbour decorated in another session comes from storage, not as raw terrain under its plants
    void storedNeighbour() {
        const std::unique_ptr<ChunkGenerator> generator = flatGenerator();
        TerrainCache cache(*generator);
        size_t loads = 0;
        cache.setStoredLoader([&loads](LevelChunk &chunk) {
            ++loads;
            if (chunk.getX() != 3 || chunk.getZ() != 0)
                return false;
            chunk.setBlock(0, 140, 0, BlockState("minecraft:gold_block"));
            chunk.setPopulated(true);
            return true;
        });
        require(cache.neighbour(3, 0).getBlock(0, 140, 0).mName == "minecraft:gold_block",
                "a stored neighbour came back as raw terrain");
        require(cache.neighbour(4, 0).getBlock(0, 140, 0).mName != "minecraft:gold_block",
                "an absent neighbour took another chunk's blocks");
        cache.neighbour(3, 0);
        cache.neighbour(4, 0);
        require(loads == 2, "storage was read again for a chunk already checked");
        require(cache.terrain(3, 0).getBlock(0, 140, 0).mName != "minecraft:gold_block",
                "raw terrain picked up a stored decoration");
    }

    // sub_1432CE930: a decoration writes into its generated neighbours at once, the next one reads it
    void writesReachNeighbours() {
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                            42);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(42, DimensionType::Overworld, cache);

        size_t checked = 0;
        for (int32_t chunkX = 0; chunkX < 4 && checked == 0; chunkX++) {
            LevelChunk chunk(chunkX, 0);
            std::vector<GeneratedBlockChange> overflow;
            std::vector<Tag> blockActors;
            std::vector<Tag> entities;
            uint64_t base = 0;
            const uint64_t own = source.populateGenerated(chunk, {}, overflow, blockActors, entities, base);
            require(own > base, "a decoration got no sequence");
            const LevelChunk raw = cache.terrain(chunkX, 0);
            const LevelChunk kept = cache.neighbour(chunkX, 0);
            bool decorated = false;
            for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y && !decorated; y++) {
                if (chunk.getBlock(8, y, 8).getHash() == raw.getBlock(8, y, 8).getHash())
                    continue;
                decorated = true;
                require(kept.getBlock(8, y, 8).getHash() == chunk.getBlock(8, y, 8).getHash(),
                        "the decorated chunk was not kept for its neighbours");
            }

            for (const GeneratedBlockChange &change: overflow) {
                const bool inArea = std::abs((change.mX >> 4) - chunkX) <= 1 && std::abs(change.mZ >> 4) <= 1;
                require(change.mSequence == (inArea ? own : 0), "an overflow block carries the wrong sequence");
                if (!inArea || change.mLayer != 0)
                    continue;

                const LevelChunk target = cache.neighbour(change.mX >> 4, change.mZ >> 4);
                require(target.getBlock(change.mX & 15, change.mY, change.mZ & 15).getHash()
                                == change.mState->getHash(),
                        "a neighbour's cached copy misses a block written into it");
                checked++;
            }
        }
        require(checked != 0, "no decoration spilled into a neighbour");
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
    check("a chunk keeps the blocks of decorations it already holds", holdsRule);
    check("a later class waits for an expected earlier neighbour", neighboursWait);
    check("neighbours never decorate at once", areaLock);
    check("stored neighbours are read from storage", storedNeighbour);
    check("a generated chunk starts from what decorations left", baseSequence);
    check("decorations write into their neighbours", writesReachNeighbours);

    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
