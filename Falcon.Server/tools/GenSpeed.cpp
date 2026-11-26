// times chunk generation and prints a hash of every generated block: a change that speeds the generators up
// must leave the hash as it was, and so must any thread count
//
// usage: FalconGenSpeed [--seed <int64>] [--at x,z] [--size n] [--repeat n] [--threads n]
//        FalconGenSpeed --stream radius [--seed <int64>] [--at x,z] [--threads n]
//   --at: lowest chunk of the area, --size: side in chunks; terrain then the area's chunks with their decorations
//   --stream: the chunk workers stream a fresh square of that radius around --at as a level asks for it after a
//   teleport, nearest first, at most MAX_OUTSTANDING_GENERATION in flight: time to the first chunk and to the
//   last, terrain built per chunk, peak memory

#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/ChunkWorker.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"
#include "World/Level/Storage/LevelStorage.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <sys/resource.h>

namespace {
    using Clock = std::chrono::steady_clock;

    double millisecondsSince(Clock::time_point start) {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    uint64_t mix(uint64_t hash, uint64_t value) {
        hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        return hash;
    }

    uint64_t hashChunk(const LevelChunk &chunk) {
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

    // the farthest terrain an area's decorations read: an area's chunks take the decorations of their 3x3, a
    // decoration those of the earlier classes around it, three classes deep, each reading its 3x3
    constexpr int32_t TERRAIN_MARGIN = 5;

    struct Run {
        double mTerrain = 0.0;
        double mDecoration = 0.0;
        uint64_t mHash = 0;
    };

    // the terrain of everything the area's decorations read, then the area's chunks on `threads` workers
    Run run(int64_t seed, int32_t originX, int32_t originZ, int32_t size, size_t threads) {
        Run result;
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                         seed);
        TerrainCache cache(*generator);

        const Clock::time_point terrainStart = Clock::now();
        for (int32_t chunkZ = originZ - TERRAIN_MARGIN; chunkZ < originZ + size + TERRAIN_MARGIN; chunkZ++) {
            for (int32_t chunkX = originX - TERRAIN_MARGIN; chunkX < originX + size + TERRAIN_MARGIN; chunkX++)
                cache.sharedTerrain(chunkX, chunkZ);
        }
        result.mTerrain = millisecondsSince(terrainStart);

        std::vector<uint64_t> hashes((size_t) (size * size), 0);
        std::atomic<int32_t> next(0);
        const Clock::time_point decorationStart = Clock::now();
        std::vector<std::thread> workers;
        // built on this thread: a generator's constructor sets shared noise, one at a time
        std::vector<std::unique_ptr<GeneratorChunkSource>> sources;
        for (size_t i = 0; i < threads; i++)
            sources.push_back(std::unique_ptr<GeneratorChunkSource>(new GeneratorChunkSource(seed, DimensionType::Overworld, cache)));
        for (size_t i = 0; i < threads; i++) {
            workers.emplace_back([&, i]() {
                GeneratorChunkSource &source = *sources[i];
                for (int32_t index = next++; index < size * size; index = next++) {
                    std::vector<Tag> blockActors;
                    std::vector<Tag> entities;
                    hashes[(size_t) index] = hashChunk(source.generate(originX + index % size, originZ + index / size,
                                                                       blockActors, entities));
                }
            });
        }
        for (std::thread &worker: workers)
            worker.join();
        result.mDecoration = millisecondsSince(decorationStart);

        for (const uint64_t hash: hashes)
            result.mHash = mix(result.mHash, hash);
        return result;
    }
}

namespace {
    int stream(int64_t seed, int32_t centreX, int32_t centreZ, int32_t radius, size_t threads) {
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                         seed);
        LevelStorage storage;
        ChunkWorker worker(*generator, storage);
        worker.start(threads);

        // one square far away first: workers and their generators warm, as on a running server
        const auto streamSquare = [&worker](int32_t atX, int32_t atZ, int32_t side, double &first) {
            std::vector<std::pair<int32_t, int32_t>> order;
            for (int32_t z = -side; z <= side; z++) {
                for (int32_t x = -side; x <= side; x++)
                    order.emplace_back(x, z);
            }
            std::stable_sort(order.begin(), order.end(), [](const auto &a, const auto &b) {
                return a.first * a.first + a.second * a.second < b.first * b.first + b.second * b.second;
            });
            const Clock::time_point start = Clock::now();
            first = -1.0;
            size_t asked = 0;
            size_t done = 0;
            while (done < order.size()) {
                while (asked < order.size()) {
                    std::vector<GeneratedBlockChange> parked;
                    if (!worker.requestLoad(atX + order[asked].first, atZ + order[asked].second, parked))
                        break;
                    asked++;
                }
                const std::vector<ChunkLoadResult> results = worker.drainCompleted();
                if (!results.empty() && first < 0.0)
                    first = millisecondsSince(start);
                done += results.size();
                if (results.empty())
                    std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
            return order.size();
        };
        double first = 0.0;
        streamSquare(centreX + 20000, centreZ, 1, first);
        const uint64_t warmTerrain = worker.getTerrainGenerations();
        const uint64_t warmDecorations = worker.getDecorationCount();
        const Clock::time_point start = Clock::now();
        const size_t chunks = streamSquare(centreX, centreZ, radius, first);
        const double total = millisecondsSince(start);
        const double terrain = (double) (worker.getTerrainGenerations() - warmTerrain) / (double) chunks;
        const double decorations = (double) (worker.getDecorationCount() - warmDecorations) / (double) chunks;
        worker.stop();

        rusage usage{};
        getrusage(RUSAGE_SELF, &usage);
        LOG_INFO(LogAreaID::Level,
                 "stream radius %d, %zu workers: first chunk %.1f ms, all %zu chunks %.0f ms (%.2f ms each), "
                 "terrain built %.2f and decorations %.2f per chunk, peak memory %.0f MB",
                 radius, threads, first, chunks, total, total / (double) chunks, terrain, decorations,
                 (double) usage.ru_maxrss / (1024.0 * 1024.0));
        BedrockLog::flush();
        return 0;
    }
}

int main(int argc, char **argv) {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    int64_t seed = -1051743253566878335LL;
    int32_t originX = 1250;
    int32_t originZ = 1250;
    int32_t size = 8;
    int32_t repeat = 3;
    size_t threads = 1;
    int32_t streamRadius = -1;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        const std::string value = argv[i + 1];
        if (flag == "--seed")
            seed = std::strtoll(value.c_str(), nullptr, 10);
        else if (flag == "--at")
            std::sscanf(value.c_str(), "%d,%d", &originX, &originZ);
        else if (flag == "--size")
            size = std::max(1, std::atoi(value.c_str()));
        else if (flag == "--repeat")
            repeat = std::max(1, std::atoi(value.c_str()));
        else if (flag == "--stream")
            streamRadius = std::max(0, std::atoi(value.c_str()));
        else if (flag == "--threads")
            threads = (size_t) std::max(1, std::atoi(value.c_str()));
    }

    BlockPaletteRegistry::getInstance().initialize();
    BiomeChunkGenDataRegistry::initialize();
    if (streamRadius >= 0)
        return stream(seed, originX, originZ, streamRadius, threads);

    const int32_t terrainChunks = (size + 2 * TERRAIN_MARGIN) * (size + 2 * TERRAIN_MARGIN);
    const int32_t decoratedChunks = size * size;
    double bestTerrain = 1e30;
    double bestDecoration = 1e30;
    uint64_t hash = 0;
    for (int32_t i = 0; i < repeat; i++) {
        const Run result = run(seed, originX, originZ, size, threads);
        if (i > 0 && result.mHash != hash) {
            LOG_ERROR(LogAreaID::Level, "hash changed between runs: %016llx then %016llx", (unsigned long long) hash,
                      (unsigned long long) result.mHash);
            BedrockLog::flush();
            return 1;
        }
        hash = result.mHash;
        bestTerrain = std::min(bestTerrain, result.mTerrain / terrainChunks);
        bestDecoration = std::min(bestDecoration, result.mDecoration / decoratedChunks);
        LOG_INFO(LogAreaID::Level, "run %d: terrain %.2f ms/chunk, decoration %.2f ms per area chunk", i + 1,
                 result.mTerrain / terrainChunks, result.mDecoration / decoratedChunks);
    }
    LOG_INFO(LogAreaID::Level, "best: terrain %.2f ms/chunk, decoration %.2f ms per area chunk, total %.2f ms per area chunk",
             bestTerrain, bestDecoration, bestTerrain * terrainChunks / decoratedChunks + bestDecoration);
    LOG_INFO(LogAreaID::Level, "hash %016llx", (unsigned long long) hash);
    // log endpoints write from a worker thread: drain the report before the process exits
    BedrockLog::flush();
    return 0;
}
