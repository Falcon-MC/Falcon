/**
 * Measures how closely Falcon's overworld generator reproduces a world generated and saved by BDS.
 *
 * The BDS database is never opened in place: LevelDB writes to any database it opens, so the "db" folder (and
 * the level.dat next to it) is copied to a temporary directory that is removed on exit. The copy is read with
 * LevelStorage, the decoder the server itself uses. The same chunks are generated and populated the way the
 * chunk workers do it: terrain, heightmap, population inside a GeneratorChunkSource, then the blocks that
 * neighbouring chunks spilled into the chunk.
 *
 * Usage:
 *   FalconWorldGenParity --db <path> --seed <int64> --area x0,z0,x1,z1 [--csv file] [--json file]
 *                        [--min-block-match pct] [--terrain-only]
 *   FalconWorldGenParity --db <path> --seed <int64> --column x,z [--terrain-only]
 *
 * --terrain-only compares the chunks as the noise fills them, before any population.
 *
 * Area corners are chunk coordinates, both inclusive; the column is a block position. Exit codes: 0 success,
 * 1 invalid input or unreadable world, 2 block match below --min-block-match.
 *
 * Definitions:
 *   - blocks compare layer 0 only; waterlogging (layer 1) is ignored;
 *   - coarse class: air, fluid (water or lava, still or flowing) or solid (everything else);
 *   - surface height: the highest Y whose layer 0 is not air, MIN_Y - 1 for an empty column;
 *   - biomes are sampled once per 4x4x4 cell, at the cell's lowest corner;
 *   - each block position belongs to the highest category of its two blocks, in the order base terrain
 *     (air, stone, deepslate, bedrock, water, lava), surface (the blocks surface builders place), features
 *     (everything else).
 *
 * JSON summary keys (stable; every percentage is in [0, 100] and is 0 when its total is 0):
 *   seed, area {x0, z0, x1, z1}, chunks {requested, compared, missing, not_finalized, unreadable},
 *   blocks {total, match_pct, coarse_match_pct},
 *   height {columns, match_pct, mean_abs_diff, max_abs_diff, histogram {"0", "1", "2", "3", "4-7", "8-15",
 *   "16-31", "32+"}},
 *   biomes {cells, match_pct},
 *   categories {base_terrain, surface, features} each {total, match_pct},
 *   y_bands [{min_y, max_y, total, match_pct}],
 *   top_block_mismatches [{expected, generated, count}], top_biome_mismatches [{expected, generated, count}].
 */

#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"
#include "World/Level/LightSystem.h"
#include "World/Level/Storage/LevelStorage.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
    const int EXIT_INVALID = 1;
    const int EXIT_BELOW_THRESHOLD = 2;
    const int64_t MAX_AREA_CHUNKS = 65536;
    const int32_t Y_BAND_HEIGHT = 32;
    const int32_t BIOME_CELL_SIZE = 4;
    const size_t TOP_PAIRS = 20;
    const int64_t HEIGHT_BUCKET_LIMITS[] = {0, 1, 2, 3, 7, 15, 31};
    const char *const HEIGHT_BUCKET_NAMES[] = {"0", "1", "2", "3", "4-7", "8-15", "16-31", "32+"};
    const size_t HEIGHT_BUCKETS = sizeof(HEIGHT_BUCKET_NAMES) / sizeof(HEIGHT_BUCKET_NAMES[0]);
    const int32_t Y_BANDS = (LevelChunk::MAX_Y - LevelChunk::MIN_Y + 1) / Y_BAND_HEIGHT;

    struct ChunkArea {
        int32_t mX0 = 0;
        int32_t mZ0 = 0;
        int32_t mX1 = 0;
        int32_t mZ1 = 0;

        bool contains(int32_t chunkX, int32_t chunkZ) const {
            return chunkX >= mX0 && chunkX <= mX1 && chunkZ >= mZ0 && chunkZ <= mZ1;
        }

        int64_t count() const {
            return ((int64_t) mX1 - mX0 + 1) * ((int64_t) mZ1 - mZ0 + 1);
        }
    };

    struct Options {
        std::string mDb;
        int64_t mSeed = 0;
        bool mHasSeed = false;
        bool mHasArea = false;
        ChunkArea mArea;
        std::string mCsv;
        std::string mJson;
        bool mHasColumn = false;
        int32_t mColumnX = 0;
        int32_t mColumnZ = 0;
        bool mHasMinimum = false;
        double mMinBlockMatch = 0.0;
        bool mTerrainOnly = false;
    };

    enum class CoarseClass {
        Air,
        Fluid,
        Solid
    };

    enum Category {
        CATEGORY_BASE_TERRAIN = 0,
        CATEGORY_SURFACE = 1,
        CATEGORY_FEATURES = 2,
        CATEGORY_COUNT = 3
    };

    const char *const CATEGORY_KEYS[CATEGORY_COUNT] = {"base_terrain", "surface", "features"};

    struct Ratio {
        uint64_t mMatched = 0;
        uint64_t mTotal = 0;

        void add(bool matched) {
            mTotal++;
            if (matched)
                mMatched++;
        }

        double percent() const {
            return mTotal == 0 ? 0.0 : 100.0 * (double) mMatched / (double) mTotal;
        }
    };

    bool parseInt64(const std::string &text, int64_t &out) {
        if (text.empty())
            return false;

        errno = 0;
        char *end = nullptr;
        const long long value = std::strtoll(text.c_str(), &end, 10);
        if (errno == ERANGE || end == text.c_str() || *end != '\0')
            return false;

        out = (int64_t) value;
        return true;
    }

    bool parseInt32List(const std::string &text, size_t count, std::vector<int32_t> &out) {
        out.clear();
        size_t start = 0;

        while (true) {
            const size_t comma = text.find(',', start);
            int64_t value = 0;
            if (!parseInt64(text.substr(start, comma == std::string::npos ? std::string::npos : comma - start), value)
                || value < INT32_MIN || value > INT32_MAX)
                return false;

            out.push_back((int32_t) value);
            if (comma == std::string::npos)
                break;
            start = comma + 1;
        }

        return out.size() == count;
    }

    void printUsage() {
        std::fprintf(stderr,
                     "Usage: FalconWorldGenParity --db <path> --seed <int64> --area x0,z0,x1,z1\n"
                     "                            [--csv file] [--json file] [--min-block-match pct]\n"
                     "                            [--terrain-only]\n"
                     "       FalconWorldGenParity --db <path> --seed <int64> --column x,z\n");
    }

    bool parseOptions(int argc, char **argv, Options &options) {
        for (int i = 1; i < argc; i++) {
            const std::string flag = argv[i];
            if (flag == "--terrain-only") {
                options.mTerrainOnly = true;
                continue;
            }

            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s needs a value\n", flag.c_str());
                return false;
            }

            const std::string value = argv[++i];
            std::vector<int32_t> numbers;

            if (flag == "--db") {
                options.mDb = value;
            } else if (flag == "--seed") {
                if (!parseInt64(value, options.mSeed)) {
                    std::fprintf(stderr, "error: --seed must be a 64-bit integer, got '%s'\n", value.c_str());
                    return false;
                }
                options.mHasSeed = true;
            } else if (flag == "--area") {
                if (!parseInt32List(value, 4, numbers)) {
                    std::fprintf(stderr, "error: --area must be x0,z0,x1,z1 in chunk coordinates, got '%s'\n",
                                 value.c_str());
                    return false;
                }
                options.mArea.mX0 = std::min(numbers[0], numbers[2]);
                options.mArea.mZ0 = std::min(numbers[1], numbers[3]);
                options.mArea.mX1 = std::max(numbers[0], numbers[2]);
                options.mArea.mZ1 = std::max(numbers[1], numbers[3]);
                options.mHasArea = true;
            } else if (flag == "--column") {
                if (!parseInt32List(value, 2, numbers)) {
                    std::fprintf(stderr, "error: --column must be x,z in block coordinates, got '%s'\n",
                                 value.c_str());
                    return false;
                }
                options.mColumnX = numbers[0];
                options.mColumnZ = numbers[1];
                options.mHasColumn = true;
            } else if (flag == "--csv") {
                options.mCsv = value;
            } else if (flag == "--json") {
                options.mJson = value;
            } else if (flag == "--min-block-match") {
                char *end = nullptr;
                options.mMinBlockMatch = std::strtod(value.c_str(), &end);
                if (value.empty() || *end != '\0' || !(options.mMinBlockMatch >= 0.0)
                    || options.mMinBlockMatch > 100.0) {
                    std::fprintf(stderr, "error: --min-block-match must be a percentage, got '%s'\n",
                                 value.c_str());
                    return false;
                }
                options.mHasMinimum = true;
            } else {
                std::fprintf(stderr, "error: unknown option %s\n", flag.c_str());
                return false;
            }
        }

        if (options.mDb.empty() || !options.mHasSeed) {
            std::fprintf(stderr, "error: --db and --seed are required\n");
            return false;
        }

        if (options.mHasArea == options.mHasColumn) {
            std::fprintf(stderr, "error: give exactly one of --area and --column\n");
            return false;
        }

        if (options.mHasArea && options.mArea.count() > MAX_AREA_CHUNKS) {
            std::fprintf(stderr, "error: the area covers %lld chunks, the limit is %lld\n",
                         (long long) options.mArea.count(), (long long) MAX_AREA_CHUNKS);
            return false;
        }

        return true;
    }

    /**
     * A private copy of the BDS world, so that LevelDB's recovery and lock files never touch the original.
     */
    class WorldCopy {
    public:
        ~WorldCopy() {
            if (mRoot.empty())
                return;

            std::error_code error;
            std::filesystem::remove_all(mRoot, error);
        }

        bool create(const std::string &dbPath, std::string &message) {
            std::error_code error;
            const std::filesystem::path source(dbPath);

            if (!std::filesystem::is_directory(source, error)) {
                message = "'" + dbPath + "' is not a directory";
                return false;
            }

            if (!std::filesystem::is_regular_file(source / "CURRENT", error)) {
                message = "'" + dbPath + "' is not a LevelDB folder (no CURRENT file)";
                return false;
            }

            const long long stamp = (long long) std::chrono::steady_clock::now().time_since_epoch().count();
            mRoot = std::filesystem::temp_directory_path(error) / ("falcon-worldgen-parity-" + std::to_string(stamp));
            if (error) {
                message = "no temporary directory: " + error.message();
                mRoot.clear();
                return false;
            }

            std::filesystem::create_directories(mRoot / LEVEL_NAME, error);
            if (!error)
                std::filesystem::copy(source, mRoot / LEVEL_NAME / "db", std::filesystem::copy_options::recursive,
                                      error);
            if (error) {
                message = "could not copy the database: " + error.message();
                return false;
            }

            const std::filesystem::path levelDat = source.parent_path() / "level.dat";
            if (std::filesystem::is_regular_file(levelDat, error))
                std::filesystem::copy_file(levelDat, mRoot / LEVEL_NAME / "level.dat", error);

            return true;
        }

        std::string getWorldsDirectory() const { return mRoot.string(); }

        static constexpr const char *LEVEL_NAME = "source";

    private:
        std::filesystem::path mRoot;
    };

    class BlockClassifier {
    public:
        BlockClassifier() {
            mAir = VanillaBlocks::AIR().getIdentifier();

            const Block base[] = {VanillaBlocks::AIR(), VanillaBlocks::STONE(), VanillaBlocks::DEEPSLATE(),
                                  VanillaBlocks::BEDROCK(), VanillaBlocks::WATER(), VanillaBlocks::FLOWING_WATER(),
                                  VanillaBlocks::LAVA(), VanillaBlocks::FLOWING_LAVA()};
            for (const Block &block: base)
                mBaseTerrain.insert(block.getIdentifier());

            const Block surface[] = {
                    VanillaBlocks::GRASS_BLOCK(), VanillaBlocks::DIRT(), VanillaBlocks::COARSE_DIRT(),
                    VanillaBlocks::PODZOL(), VanillaBlocks::MYCELIUM(), VanillaBlocks::MUD(),
                    VanillaBlocks::SAND(), VanillaBlocks::RED_SAND(), VanillaBlocks::SANDSTONE(),
                    VanillaBlocks::RED_SANDSTONE(), VanillaBlocks::GRAVEL(), VanillaBlocks::CALCITE(),
                    VanillaBlocks::SNOW_LAYER(), VanillaBlocks::SNOW(), VanillaBlocks::POWDER_SNOW(),
                    VanillaBlocks::ICE(), VanillaBlocks::PACKED_ICE(), VanillaBlocks::HARDENED_CLAY(),
                    VanillaBlocks::WHITE_TERRACOTTA(), VanillaBlocks::ORANGE_TERRACOTTA(),
                    VanillaBlocks::MAGENTA_TERRACOTTA(), VanillaBlocks::LIGHT_BLUE_TERRACOTTA(),
                    VanillaBlocks::YELLOW_TERRACOTTA(), VanillaBlocks::LIME_TERRACOTTA(),
                    VanillaBlocks::PINK_TERRACOTTA(), VanillaBlocks::GRAY_TERRACOTTA(),
                    VanillaBlocks::LIGHT_GRAY_TERRACOTTA(), VanillaBlocks::CYAN_TERRACOTTA(),
                    VanillaBlocks::PURPLE_TERRACOTTA(), VanillaBlocks::BLUE_TERRACOTTA(),
                    VanillaBlocks::BROWN_TERRACOTTA(), VanillaBlocks::GREEN_TERRACOTTA(),
                    VanillaBlocks::RED_TERRACOTTA(), VanillaBlocks::BLACK_TERRACOTTA()};
            for (const Block &block: surface)
                mSurface.insert(block.getIdentifier());
        }

        bool isAir(const BlockState &state) const {
            return state.mName == mAir;
        }

        CoarseClass coarse(const BlockState &state) const {
            if (isAir(state))
                return CoarseClass::Air;

            return LiquidView(state).isLiquid() ? CoarseClass::Fluid : CoarseClass::Solid;
        }

        // terrain shape of the blocks the noise fills and ores replace: open, fluid, solid; -1 for anything else
        int shape(const BlockState &state) const {
            if (isAir(state))
                return 0;

            if (LiquidView(state).isLiquid())
                return 1;

            const std::string &name = state.mName;
            static const char *const SOLID[] = {"minecraft:stone", "minecraft:deepslate", "minecraft:granite",
                                                 "minecraft:diorite", "minecraft:andesite", "minecraft:tuff",
                                                 "minecraft:dirt", "minecraft:gravel", "minecraft:bedrock",
                                                 "minecraft:raw_iron_block", "minecraft:raw_copper_block"};
            for (const char *solid: SOLID) {
                if (name == solid)
                    return 2;
            }

            return name.size() > 4 && name.compare(name.size() - 4, 4, "_ore") == 0 ? 2 : -1;
        }

        Category category(const BlockState &state) const {
            if (mBaseTerrain.count(state.mName) != 0)
                return CATEGORY_BASE_TERRAIN;

            return mSurface.count(state.mName) != 0 ? CATEGORY_SURFACE : CATEGORY_FEATURES;
        }

    private:
        std::string mAir;
        std::unordered_set<std::string> mBaseTerrain;
        std::unordered_set<std::string> mSurface;
    };

    std::string describeState(const BlockState &state) {
        if (state.mStates.isCompound() && state.mStates.isEmpty())
            return state.mName;

        return state.mName + " " + state.mStates.toString();
    }

    int32_t surfaceHeight(const LevelChunk &chunk, int x, int z, const BlockClassifier &classifier) {
        for (int32_t y = LevelChunk::MAX_Y; y >= LevelChunk::MIN_Y; y--) {
            if (!classifier.isAir(chunk.getBlock(x, y, z)))
                return y;
        }

        return LevelChunk::MIN_Y - 1;
    }

    class BiomeNames {
    public:
        BiomeNames() {
            for (const std::string &name: BiomeChunkGenDataRegistry::getBiomeNames()) {
                const int32_t id = BiomeChunkGenDataRegistry::getBiomeId(name);
                if (id >= 0)
                    mNames[(uint32_t) id] = name;
            }
        }

        std::string get(uint32_t id) const {
            const std::unordered_map<uint32_t, std::string>::const_iterator found = mNames.find(id);
            return found == mNames.end() ? "#" + std::to_string(id) : found->second;
        }

    private:
        std::unordered_map<uint32_t, std::string> mNames;
    };

    int64_t packChunk(int32_t chunkX, int32_t chunkZ) {
        return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    }

    /**
     * Generates and populates every chunk of an area the way the chunk workers do, and hands each one over once
     * all of its neighbours are populated, so that the blocks they spilled into it are present. Chunks are
     * produced row by row, which keeps only about two rows in memory.
     */
    void generateArea(int64_t seed, const ChunkArea &area, bool terrainOnly,
                      const std::function<void(LevelChunk &)> &visit) {
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                         seed);
        GeneratorChunkSource source(seed, DimensionType::Overworld);
        std::unordered_map<int64_t, LevelChunk> waiting;
        std::unordered_map<int64_t, std::vector<GeneratedBlockChange>> spilled;

        for (int32_t chunkZ = area.mZ0 - 1; chunkZ <= area.mZ1 + 1; chunkZ++) {
            for (int32_t chunkX = area.mX0 - 1; chunkX <= area.mX1 + 1; chunkX++) {
                LevelChunk chunk(chunkX, chunkZ);
                chunk.setDimension(DimensionType::Overworld);
                generator->generate(chunk);
                if (!chunk.hasHeightmap())
                    LightSystem::computeHeightmap(chunk);

                std::vector<GeneratedBlockChange> overflow;
                std::vector<Tag> blockActors;
                if (!terrainOnly)
                    source.populate(chunk, overflow, blockActors);
                chunk.setPopulated(true);

                for (GeneratedBlockChange &change: overflow) {
                    if (change.mY < LevelChunk::MIN_Y || change.mY > LevelChunk::MAX_Y
                        || !area.contains(change.mX >> 4, change.mZ >> 4))
                        continue;

                    spilled[packChunk(change.mX >> 4, change.mZ >> 4)].push_back(std::move(change));
                }

                if (area.contains(chunkX, chunkZ))
                    waiting.emplace(packChunk(chunkX, chunkZ), std::move(chunk));

                // The chunk diagonally behind has just had its last neighbour populated.
                if (!area.contains(chunkX - 1, chunkZ - 1))
                    continue;

                const int64_t key = packChunk(chunkX - 1, chunkZ - 1);
                LevelChunk &finished = waiting.at(key);
                const std::unordered_map<int64_t, std::vector<GeneratedBlockChange>>::iterator changes =
                        spilled.find(key);
                if (changes != spilled.end()) {
                    for (const GeneratedBlockChange &change: changes->second)
                        finished.setBlock(change.mX & 15, change.mY, change.mZ & 15, change.mState);
                    spilled.erase(changes);
                }

                visit(finished);
                waiting.erase(key);
            }
        }
    }

    enum class LoadResult {
        Loaded,
        Missing,
        NotFinalized,
        Unreadable
    };

    LoadResult loadExpected(LevelStorage &storage, LevelChunk &chunk) {
        chunk.setDimension(DimensionType::Overworld);

        try {
            if (!storage.loadChunk(chunk))
                return LoadResult::Missing;
        } catch (const std::exception &exception) {
            std::fprintf(stderr, "warning: chunk %d %d is unreadable: %s\n", chunk.getX(), chunk.getZ(),
                         exception.what());
            return LoadResult::Unreadable;
        }

        return chunk.isPopulated() ? LoadResult::Loaded : LoadResult::NotFinalized;
    }

    struct ChunkScore {
        int32_t mX = 0;
        int32_t mZ = 0;
        Ratio mBlocks;
        Ratio mHeights;
        Ratio mBiomes;
    };

    using PairCounts = std::map<std::pair<std::string, std::string>, uint64_t>;

    class ParityReport {
    public:
        ParityReport(const BlockClassifier &classifier, const BiomeNames &biomes)
                : mClassifier(classifier), mBiomeNames(biomes), mBands((size_t) Y_BANDS),
                  mHeightHistogram(HEIGHT_BUCKETS, 0) {}

        ChunkScore compare(const LevelChunk &expected, const LevelChunk &generated) {
            ChunkScore score;
            score.mX = expected.getX();
            score.mZ = expected.getZ();

            for (int x = 0; x < 16; x++) {
                for (int z = 0; z < 16; z++) {
                    for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y; y++)
                        _compareBlock(expected.getBlock(x, y, z), generated.getBlock(x, y, z),
                                      expected.getX() * 16 + x, y, expected.getZ() * 16 + z, score);

                    const int64_t difference = std::llabs((int64_t) surfaceHeight(expected, x, z, mClassifier)
                                                          - surfaceHeight(generated, x, z, mClassifier));
                    score.mHeights.add(difference == 0);
                    mHeightDifferenceSum += (uint64_t) difference;
                    mMaxHeightDifference = std::max(mMaxHeightDifference, difference);
                    size_t bucket = 0;
                    while (bucket < HEIGHT_BUCKETS - 1 && difference > HEIGHT_BUCKET_LIMITS[bucket])
                        bucket++;
                    mHeightHistogram[bucket]++;
                }
            }

            for (int x = 0; x < 16; x += BIOME_CELL_SIZE) {
                for (int z = 0; z < 16; z += BIOME_CELL_SIZE) {
                    for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y; y += BIOME_CELL_SIZE) {
                        const uint32_t expectedBiome = expected.getBiomeAt(x, y, z);
                        const uint32_t generatedBiome = generated.getBiomeAt(x, y, z);
                        score.mBiomes.add(expectedBiome == generatedBiome);
                        mBiomesByExpected[expectedBiome].add(expectedBiome == generatedBiome);
                        if (expectedBiome != generatedBiome)
                            mBiomeMismatches[{mBiomeNames.get(expectedBiome), mBiomeNames.get(generatedBiome)}]++;
                    }
                }
            }

            _merge(mBlocks, score.mBlocks);
            _merge(mHeights, score.mHeights);
            _merge(mBiomes, score.mBiomes);
            mChunks.push_back(score);
            return score;
        }

        const Ratio &getBlocks() const { return mBlocks; }

        void printText(int64_t seed, const ChunkArea &area, const uint64_t *skipped) const {
            std::printf("World generation parity, seed %lld, chunks %d,%d to %d,%d\n", (long long) seed, area.mX0,
                        area.mZ0, area.mX1, area.mZ1);
            std::printf("Chunks: %lld requested, %zu compared, %llu never saved by BDS, %llu not finalized, "
                        "%llu unreadable\n\n", (long long) area.count(), mChunks.size(),
                        (unsigned long long) skipped[0], (unsigned long long) skipped[1],
                        (unsigned long long) skipped[2]);

            std::printf("Blocks: %.3f%% identical, %.3f%% same class (air/fluid/solid) over %llu blocks\n",
                        mBlocks.percent(), mCoarse.percent(), (unsigned long long) mBlocks.mTotal);
            std::printf("Surface height: %.3f%% equal, mean |diff| %.3f, max |diff| %lld over %llu columns\n",
                        mHeights.percent(), _meanHeightDifference(), (long long) mMaxHeightDifference,
                        (unsigned long long) mHeights.mTotal);
            std::printf("  |diff|  columns\n");
            for (size_t bucket = 0; bucket < HEIGHT_BUCKETS; bucket++)
                std::printf("  %-6s  %llu\n", HEIGHT_BUCKET_NAMES[bucket],
                            (unsigned long long) mHeightHistogram[bucket]);
            std::printf("Biomes: %.3f%% equal over %llu 4x4x4 cells\n\n", mBiomes.percent(),
                        (unsigned long long) mBiomes.mTotal);

            static const char *const SHAPES[3] = {"open", "fluid", "solid"};
            std::printf("Terrain shape: %.3f%% same (open/fluid/solid) over %llu blocks\n", mShape.percent(),
                        (unsigned long long) mShape.mTotal);
            std::vector<std::pair<std::pair<int, int>, ShapeMismatch>> shapes(mShapeMismatches.begin(),
                                                                              mShapeMismatches.end());
            std::sort(shapes.begin(), shapes.end(), [](const std::pair<std::pair<int, int>, ShapeMismatch> &a,
                                                       const std::pair<std::pair<int, int>, ShapeMismatch> &b) {
                return a.second.mCount > b.second.mCount;
            });
            for (size_t i = 0; i < shapes.size() && i < 16; i++) {
                const int pair = shapes[i].first.first;
                const int32_t minY = LevelChunk::MIN_Y + shapes[i].first.second * Y_BAND_HEIGHT;
                std::printf("  %10llu  %-5s -> %-5s y %4d..%-4d", (unsigned long long) shapes[i].second.mCount,
                            SHAPES[pair / 3], SHAPES[pair % 3], minY, minY + Y_BAND_HEIGHT - 1);
                for (const std::array<int32_t, 3> &sample: shapes[i].second.mSamples)
                    std::printf("  (%d %d %d)", sample[0], sample[1], sample[2]);
                std::printf("\n");
            }
            std::printf("\n");

            std::printf("By category        blocks       match\n");
            for (int category = 0; category < CATEGORY_COUNT; category++)
                std::printf("  %-14s %12llu  %8.3f%%\n", CATEGORY_KEYS[category],
                            (unsigned long long) mCategories[category].mTotal, mCategories[category].percent());

            std::printf("\nBy Y band          blocks       match\n");
            for (size_t band = 0; band < mBands.size(); band++) {
                const int32_t minY = LevelChunk::MIN_Y + (int32_t) band * Y_BAND_HEIGHT;
                std::printf("  %4d..%-4d    %12llu  %8.3f%%\n", minY, minY + Y_BAND_HEIGHT - 1,
                            (unsigned long long) mBands[band].mTotal, mBands[band].percent());
            }

            std::printf("\nTop block mismatches (expected -> generated)\n");
            for (const std::pair<std::pair<std::string, std::string>, uint64_t> &entry: _top(mBlockMismatches))
                std::printf("  %10llu  %s -> %s\n", (unsigned long long) entry.second, entry.first.first.c_str(),
                            entry.first.second.c_str());

            std::printf("\nBiome accuracy by expected biome\n");
            std::vector<std::pair<uint32_t, Ratio>> byBiome(mBiomesByExpected.begin(), mBiomesByExpected.end());
            std::sort(byBiome.begin(), byBiome.end(),
                      [](const std::pair<uint32_t, Ratio> &a, const std::pair<uint32_t, Ratio> &b) {
                          return a.second.mTotal > b.second.mTotal;
                      });
            for (const std::pair<uint32_t, Ratio> &entry: byBiome)
                std::printf("  %-28s %10llu cells  %8.3f%%\n", mBiomeNames.get(entry.first).c_str(),
                            (unsigned long long) entry.second.mTotal, entry.second.percent());

            std::printf("\nTop biome confusions (expected -> generated)\n");
            for (const std::pair<std::pair<std::string, std::string>, uint64_t> &entry: _top(mBiomeMismatches))
                std::printf("  %10llu  %s -> %s\n", (unsigned long long) entry.second, entry.first.first.c_str(),
                            entry.first.second.c_str());
        }

        bool writeCsv(const std::string &path) const {
            std::ofstream out(path);
            if (!out.is_open())
                return false;

            out << "chunk_x,chunk_z,block_match_pct,height_match_pct,biome_match_pct\n";
            char line[128];
            for (const ChunkScore &score: mChunks) {
                std::snprintf(line, sizeof(line), "%d,%d,%.3f,%.3f,%.3f\n", score.mX, score.mZ,
                              score.mBlocks.percent(), score.mHeights.percent(), score.mBiomes.percent());
                out << line;
            }

            return out.good();
        }

        bool writeJson(const std::string &path, int64_t seed, const ChunkArea &area, const uint64_t *skipped) const {
            std::ofstream out(path);
            if (!out.is_open())
                return false;

            char buffer[256];
            std::snprintf(buffer, sizeof(buffer),
                          "{\n  \"seed\": %lld,\n  \"area\": {\"x0\": %d, \"z0\": %d, \"x1\": %d, \"z1\": %d},\n"
                          "  \"chunks\": {\"requested\": %lld, \"compared\": %zu, \"missing\": %llu, "
                          "\"not_finalized\": %llu, \"unreadable\": %llu},\n",
                          (long long) seed, area.mX0, area.mZ0, area.mX1, area.mZ1, (long long) area.count(),
                          mChunks.size(), (unsigned long long) skipped[0], (unsigned long long) skipped[1],
                          (unsigned long long) skipped[2]);
            out << buffer;
            std::snprintf(buffer, sizeof(buffer),
                          "  \"blocks\": {\"total\": %llu, \"match_pct\": %.4f, \"coarse_match_pct\": %.4f},\n",
                          (unsigned long long) mBlocks.mTotal, mBlocks.percent(), mCoarse.percent());
            out << buffer;
            std::snprintf(buffer, sizeof(buffer),
                          "  \"height\": {\"columns\": %llu, \"match_pct\": %.4f, \"mean_abs_diff\": %.4f, "
                          "\"max_abs_diff\": %lld, \"histogram\": {",
                          (unsigned long long) mHeights.mTotal, mHeights.percent(), _meanHeightDifference(),
                          (long long) mMaxHeightDifference);
            out << buffer;
            for (size_t bucket = 0; bucket < HEIGHT_BUCKETS; bucket++)
                out << (bucket == 0 ? "" : ", ") << "\"" << HEIGHT_BUCKET_NAMES[bucket] << "\": "
                    << mHeightHistogram[bucket];
            out << "}},\n";
            std::snprintf(buffer, sizeof(buffer), "  \"biomes\": {\"cells\": %llu, \"match_pct\": %.4f},\n",
                          (unsigned long long) mBiomes.mTotal, mBiomes.percent());
            out << buffer;

            out << "  \"categories\": {";
            for (int category = 0; category < CATEGORY_COUNT; category++) {
                std::snprintf(buffer, sizeof(buffer), "%s\"%s\": {\"total\": %llu, \"match_pct\": %.4f}",
                              category == 0 ? "" : ", ", CATEGORY_KEYS[category],
                              (unsigned long long) mCategories[category].mTotal, mCategories[category].percent());
                out << buffer;
            }
            out << "},\n  \"y_bands\": [";
            for (size_t band = 0; band < mBands.size(); band++) {
                const int32_t minY = LevelChunk::MIN_Y + (int32_t) band * Y_BAND_HEIGHT;
                std::snprintf(buffer, sizeof(buffer),
                              "%s{\"min_y\": %d, \"max_y\": %d, \"total\": %llu, \"match_pct\": %.4f}",
                              band == 0 ? "" : ", ", minY, minY + Y_BAND_HEIGHT - 1,
                              (unsigned long long) mBands[band].mTotal, mBands[band].percent());
                out << buffer;
            }
            out << "],\n  \"top_block_mismatches\": ";
            _writePairs(out, mBlockMismatches);
            out << ",\n  \"top_biome_mismatches\": ";
            _writePairs(out, mBiomeMismatches);
            out << "\n}\n";

            return out.good();
        }

    private:
        static void _merge(Ratio &into, const Ratio &from) {
            into.mMatched += from.mMatched;
            into.mTotal += from.mTotal;
        }

        void _compareBlock(const BlockState &expected, const BlockState &generated, int32_t x, int32_t y, int32_t z,
                           ChunkScore &score) {
            const bool matched = expected == generated;
            const int expectedShape = mClassifier.shape(expected);
            const int generatedShape = mClassifier.shape(generated);
            if (expectedShape >= 0 && generatedShape >= 0)
                mShape.add(expectedShape == generatedShape);
            if (expectedShape >= 0 && generatedShape >= 0 && expectedShape != generatedShape) {
                ShapeMismatch &mismatch = mShapeMismatches[{expectedShape * 3 + generatedShape,
                                                            (y - LevelChunk::MIN_Y) / Y_BAND_HEIGHT}];
                if (mismatch.mCount++ < 3)
                    mismatch.mSamples.push_back({x, y, z});
            }
            score.mBlocks.add(matched);
            mCoarse.add(mClassifier.coarse(expected) == mClassifier.coarse(generated));
            mCategories[std::max(mClassifier.category(expected), mClassifier.category(generated))].add(matched);
            mBands[(size_t) ((y - LevelChunk::MIN_Y) / Y_BAND_HEIGHT)].add(matched);

            if (matched)
                return;

            if (expected.mName == generated.mName) {
                mBlockMismatches[{describeState(expected), describeState(generated)}]++;
                return;
            }
            mBlockMismatches[{expected.mName, generated.mName}]++;
        }

        double _meanHeightDifference() const {
            return mHeights.mTotal == 0 ? 0.0 : (double) mHeightDifferenceSum / (double) mHeights.mTotal;
        }

        static std::vector<std::pair<std::pair<std::string, std::string>, uint64_t>> _top(const PairCounts &counts) {
            std::vector<std::pair<std::pair<std::string, std::string>, uint64_t>> sorted(counts.begin(), counts.end());
            std::sort(sorted.begin(), sorted.end(),
                      [](const std::pair<std::pair<std::string, std::string>, uint64_t> &a,
                         const std::pair<std::pair<std::string, std::string>, uint64_t> &b) {
                          return a.second != b.second ? a.second > b.second : a.first < b.first;
                      });
            if (sorted.size() > TOP_PAIRS)
                sorted.resize(TOP_PAIRS);
            return sorted;
        }

        static std::string _jsonString(const std::string &text) {
            std::string escaped = "\"";
            for (char character: text) {
                if (character == '"' || character == '\\') {
                    escaped.push_back('\\');
                    escaped.push_back(character);
                } else if ((unsigned char) character < 0x20) {
                    char code[8];
                    std::snprintf(code, sizeof(code), "\\u%04x", (unsigned) (unsigned char) character);
                    escaped += code;
                } else {
                    escaped.push_back(character);
                }
            }
            return escaped + "\"";
        }

        static void _writePairs(std::ofstream &out, const PairCounts &counts) {
            out << "[";
            bool first = true;
            for (const std::pair<std::pair<std::string, std::string>, uint64_t> &entry: _top(counts)) {
                out << (first ? "" : ", ") << "{\"expected\": " << _jsonString(entry.first.first)
                    << ", \"generated\": " << _jsonString(entry.first.second) << ", \"count\": " << entry.second
                    << "}";
                first = false;
            }
            out << "]";
        }

        const BlockClassifier &mClassifier;
        const BiomeNames &mBiomeNames;
        Ratio mBlocks;
        Ratio mCoarse;
        Ratio mShape;

        struct ShapeMismatch {
            uint64_t mCount = 0;
            std::vector<std::array<int32_t, 3>> mSamples;
        };

        std::map<std::pair<int, int>, ShapeMismatch> mShapeMismatches;
        Ratio mHeights;
        Ratio mBiomes;
        Ratio mCategories[CATEGORY_COUNT];
        std::vector<Ratio> mBands;
        std::vector<uint64_t> mHeightHistogram;
        uint64_t mHeightDifferenceSum = 0;
        int64_t mMaxHeightDifference = 0;
        PairCounts mBlockMismatches;
        PairCounts mBiomeMismatches;
        std::map<uint32_t, Ratio> mBiomesByExpected;
        std::vector<ChunkScore> mChunks;
    };

    int dumpColumn(LevelStorage &storage, const Options &options, const BiomeNames &biomes) {
        const int32_t chunkX = options.mColumnX >> 4;
        const int32_t chunkZ = options.mColumnZ >> 4;
        LevelChunk expected(chunkX, chunkZ);

        const LoadResult loaded = loadExpected(storage, expected);
        if (loaded == LoadResult::Missing || loaded == LoadResult::Unreadable) {
            std::fprintf(stderr, "error: chunk %d %d holding column %d,%d is %s in the BDS world\n", chunkX, chunkZ,
                         options.mColumnX, options.mColumnZ, loaded == LoadResult::Missing ? "absent" : "unreadable");
            return EXIT_INVALID;
        }
        if (loaded == LoadResult::NotFinalized)
            std::printf("note: BDS has not finished populating chunk %d %d\n", chunkX, chunkZ);

        ChunkArea area;
        area.mX0 = area.mX1 = chunkX;
        area.mZ0 = area.mZ1 = chunkZ;
        const int x = options.mColumnX & 15;
        const int z = options.mColumnZ & 15;

        generateArea(options.mSeed, area, options.mTerrainOnly, [&](LevelChunk &generated) {
            std::printf("Column %d,%d (chunk %d %d), seed %lld; '*' marks a difference\n", options.mColumnX,
                        options.mColumnZ, chunkX, chunkZ, (long long) options.mSeed);
            for (int32_t y = LevelChunk::MAX_Y; y >= LevelChunk::MIN_Y; y--) {
                const BlockState &expectedBlock = expected.getBlock(x, y, z);
                const BlockState &generatedBlock = generated.getBlock(x, y, z);
                const uint32_t expectedBiome = expected.getBiomeAt(x, y, z);
                const uint32_t generatedBiome = generated.getBiomeAt(x, y, z);
                std::printf("%c%c %4d  %-40s %-40s %-22s %s\n", expectedBlock == generatedBlock ? ' ' : '*',
                            expectedBiome == generatedBiome ? ' ' : '*', y, describeState(expectedBlock).c_str(),
                            describeState(generatedBlock).c_str(), biomes.get(expectedBiome).c_str(),
                            biomes.get(generatedBiome).c_str());
            }
        });

        return 0;
    }

    int run(const Options &options) {
        WorldCopy copy;
        std::string message;
        if (!copy.create(options.mDb, message)) {
            std::fprintf(stderr, "error: %s\n", message.c_str());
            return EXIT_INVALID;
        }

        LevelStorage storage;
        if (!storage.open(copy.getWorldsDirectory(), WorldCopy::LEVEL_NAME, Dimension::toId(DimensionType::Overworld))) {
            std::fprintf(stderr, "error: could not open '%s' as a LevelDB database\n", options.mDb.c_str());
            return EXIT_INVALID;
        }

        Tag levelDat;
        if (storage.readLevelDat(levelDat) && levelDat.contains("RandomSeed")
            && levelDat.getLong("RandomSeed") != options.mSeed)
            std::fprintf(stderr, "warning: level.dat records seed %lld, comparing with --seed %lld\n",
                         (long long) levelDat.getLong("RandomSeed"), (long long) options.mSeed);

        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        const BiomeNames biomes;

        if (options.mHasColumn)
            return dumpColumn(storage, options, biomes);

        const BlockClassifier classifier;
        ParityReport report(classifier, biomes);
        uint64_t skipped[3] = {0, 0, 0};

        generateArea(options.mSeed, options.mArea, options.mTerrainOnly, [&](LevelChunk &generated) {
            LevelChunk expected(generated.getX(), generated.getZ());
            const LoadResult loaded = loadExpected(storage, expected);
            if (loaded != LoadResult::Loaded) {
                skipped[(int) loaded - 1]++;
                return;
            }

            report.compare(expected, generated);
        });

        if (report.getBlocks().mTotal == 0) {
            std::fprintf(stderr, "error: BDS saved no finalized chunk in this area\n");
            return EXIT_INVALID;
        }

        report.printText(options.mSeed, options.mArea, skipped);

        if (!options.mCsv.empty() && !report.writeCsv(options.mCsv)) {
            std::fprintf(stderr, "error: could not write %s\n", options.mCsv.c_str());
            return EXIT_INVALID;
        }

        if (!options.mJson.empty() && !report.writeJson(options.mJson, options.mSeed, options.mArea, skipped)) {
            std::fprintf(stderr, "error: could not write %s\n", options.mJson.c_str());
            return EXIT_INVALID;
        }

        if (options.mHasMinimum && report.getBlocks().percent() < options.mMinBlockMatch) {
            std::fprintf(stderr, "block match %.3f%% is below the required %.3f%%\n", report.getBlocks().percent(),
                         options.mMinBlockMatch);
            return EXIT_BELOW_THRESHOLD;
        }

        return 0;
    }
}

int main(int argc, char **argv) {
    Options options;
    if (!parseOptions(argc, argv, options)) {
        printUsage();
        return EXIT_INVALID;
    }

    try {
        return run(options);
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "error: %s\n", exception.what());
        return EXIT_INVALID;
    }
}
