// compares Falcon's generators against a world generated and saved by BDS
//
// the BDS "db" (and its level.dat) is copied to a temp dir removed on exit: LevelDB writes to any database it
// opens; read with LevelStorage; chunks are generated like the chunk workers do: terrain, heightmap, population
// in a GeneratorChunkSource on the shared TerrainCache
//
// usage:
//   FalconWorldGenParity --db <path> --seed <int64> --area x0,z0,x1,z1 [--csv file] [--json file]
//                        [--min-block-match pct] [--terrain-only] [--dimension 0|1|2] [--blocks file]
//                        [--order rows|spiral|reverse|random:<n>] [--threads n] [--behavior-packs dir]
//   FalconWorldGenParity --db <path> --seed <int64> --column x,z [--terrain-only]
//   FalconWorldGenParity --db <path> --seed <int64> --far [--min-block-match pct] [--terrain-only]
//
// --dimension: 0 overworld (default), 1 nether, 2 end; only its build range is compared
// populated Nether comparisons report positive placement chunk overlap for three structures
// --mineshaft-markers minY,maxY: exact states at the union of mineshaft-marker positions in an overworld area
// --min-mineshaft-marker-match pct: exit 2 when that score falls below the threshold
// --terrain-only: chunks as the noise fills them, before population
// --order, --threads: the order the area's chunks are asked for and the workers generating them; a chunk must
//   come out the same for every order
// --blocks: one "x y z expected generated" line per block that differs or is not plain terrain, for scripts
// --block-states: append tab-separated expected and generated states to the block dump
// --dump: both worlds of every visited chunk in a binary file for scripts, its strings in <file>.strings;
//   per chunk x, z (int32) and the BDS load result (uint8: 0 loaded, 1 missing, 2 not finalized, 3 unreadable),
//   then for a loaded chunk BDS then Falcon: layer 0 and layer 1 string indices (uint16, x then z then y),
//   biome name indices per 4x4x4 cell (uint16, same order), structure boxes (uint32 count, each a uint16 name
//   index and six int32 min x y z, max x y z)
// --far: 6x6 chunks at +-1000 and +-10000 on both axes, one report each: climate-only bugs hide near the origin
// area corners: chunk coordinates, inclusive; column: a block position
// exit codes: 0 success, 1 invalid input or unreadable world, 2 block match below --min-block-match
//
// definitions:
//   - blocks: layer 0 only, waterlogging (layer 1) ignored
//   - coarse class: air, fluid (water or lava, still or flowing), solid (everything else)
//   - surface height: highest Y with non-air layer 0, MIN_Y - 1 for an empty column
//   - biomes: one sample per 4x4x4 cell at its lowest corner; above the highest non-empty BDS section, read from
//     that section: the game saves it in place of the sections above
//   - ore spills: an underground ore rule block against another such block or a host rock (stone, deepslate);
//     stone vs deepslate stays a terrain mismatch; reported apart, not excused: two BDS runs differ by only a
//     few ore blocks (writes into a neighbour still generating are dropped), the rest comes from decoration order
//   - category: the highest of the two blocks', in order base terrain (air, stone, deepslate, bedrock, water,
//     lava), surface (surface builder blocks), features (everything else)
//
// JSON summary keys (stable; percentages in [0, 100], 0 when their total is 0):
//   seed, area {x0, z0, x1, z1}, chunks {requested, compared, missing, not_finalized, unreadable},
//   blocks {total, match_pct, coarse_match_pct, ore_spills, outside_ore_spills_match_pct},
//   height {columns, match_pct, mean_abs_diff, max_abs_diff, histogram {"0", "1", "2", "3", "4-7", "8-15",
//   "16-31", "32+"}},
//   biomes {cells, match_pct},
//   categories {base_terrain, surface, features} each {total, match_pct},
//   y_bands [{min_y, max_y, total, match_pct}],
//   top_block_mismatches [{expected, generated, count}], top_biome_mismatches [{expected, generated, count}]

#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Scripting/BehaviorPackManager.h"
#include "Server/ServerPaths.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"
#include "World/Level/LevelGen/Structure/JigsawStructures.h"
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
#include <atomic>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
    // report lines gathered in a loop are built here, then logged once
    std::string formatted(const char *format, ...) BEDROCK_LOG_PRINTF_FORMAT(1, 2);

    std::string formatted(const char *format, ...) {
        va_list args;
        va_start(args, format);
        va_list argsCopy;
        va_copy(argsCopy, args);
        const int length = std::vsnprintf(nullptr, 0, format, argsCopy);
        va_end(argsCopy);

        std::string out(length > 0 ? (size_t) length : 0, '\0');
        if (length > 0)
            std::vsnprintf(&out[0], (size_t) length + 1, format, args);
        va_end(args);
        return out;
    }

    const int EXIT_INVALID = 1;
    const int EXIT_BELOW_THRESHOLD = 2;
    const int64_t MAX_AREA_CHUNKS = 65536;
    const int32_t Y_BAND_HEIGHT = 32;
    const int32_t BIOME_CELL_SIZE = 4;
    const size_t TOP_PAIRS = 20;
    const int64_t HEIGHT_BUCKET_LIMITS[] = {0, 1, 2, 3, 7, 15, 31};
    const char *const HEIGHT_BUCKET_NAMES[] = {"0", "1", "2", "3", "4-7", "8-15", "16-31", "32+"};
    const size_t HEIGHT_BUCKETS = sizeof(HEIGHT_BUCKET_NAMES) / sizeof(HEIGHT_BUCKET_NAMES[0]);

    // nether and end share the overworld chunk layout; only their build range is compared:
    // the empty sections around would inflate the match
    DimensionType gDimension = DimensionType::Overworld;
    int32_t gMinY = LevelChunk::MIN_Y;
    int32_t gMaxY = LevelChunk::MAX_Y;

    void selectDimension(DimensionType dimension) {
        gDimension = dimension;
        gMinY = dimension == DimensionType::Overworld ? LevelChunk::MIN_Y : 0;
        gMaxY = dimension == DimensionType::Overworld ? LevelChunk::MAX_Y
                                                      : dimension == DimensionType::Nether ? 127 : 255;
    }

    int32_t yBandCount() {
        return (gMaxY - gMinY + 1) / Y_BAND_HEIGHT;
    }

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
        std::string mBehaviorPacks;
        int64_t mSeed = 0;
        bool mHasSeed = false;
        bool mHasArea = false;
        ChunkArea mArea;
        std::string mCsv;
        std::string mJson;
        std::string mBlocks;
        std::string mDump;
        bool mDumpBlockStates = false;
        bool mHasColumn = false;
        bool mFar = false;
        int32_t mColumnX = 0;
        int32_t mColumnZ = 0;
        bool mHasMinimum = false;
        double mMinBlockMatch = 0.0;
        bool mTerrainOnly = false;
        bool mMineshaftMarkers = false;
        bool mHasMineshaftMinimum = false;
        double mMinMineshaftMatch = 0.0;
        int32_t mMineshaftMinY = 0;
        int32_t mMineshaftMaxY = 0;
        std::string mOrder = "rows";
        size_t mThreads = 1;
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
        LOG_ERROR(LogAreaID::Level, "Usage: FalconWorldGenParity --db <path> --seed <int64> --area x0,z0,x1,z1");
        LOG_ERROR(LogAreaID::Level, "                            [--csv file] [--json file] [--min-block-match pct]");
        LOG_ERROR(LogAreaID::Level, "                            [--terrain-only] [--dimension 0|1|2] [--blocks file]");
        LOG_ERROR(LogAreaID::Level, "                            [--block-states] [--dump file]");
        LOG_ERROR(LogAreaID::Level, "                            [--order rows|spiral|reverse|random:<n>] [--threads n] [--behavior-packs dir]");
        LOG_ERROR(LogAreaID::Level, "                            [--mineshaft-markers minY,maxY] [--min-mineshaft-marker-match pct]");
        LOG_ERROR(LogAreaID::Level, "       FalconWorldGenParity --db <path> --seed <int64> --column x,z");
        LOG_ERROR(LogAreaID::Level, "       FalconWorldGenParity --db <path> --seed <int64> --far");
    }

    bool parseOptions(int argc, char **argv, Options &options) {
        for (int i = 1; i < argc; i++) {
            const std::string flag = argv[i];
            if (flag == "--far") {
                options.mFar = true;
                continue;
            }

            if (flag == "--terrain-only") {
                options.mTerrainOnly = true;
                continue;
            }
            if (flag == "--block-states") {
                options.mDumpBlockStates = true;
                continue;
            }

            if (i + 1 >= argc) {
                LOG_ERROR(LogAreaID::Level, "%s needs a value", flag.c_str());
                return false;
            }

            const std::string value = argv[++i];
            std::vector<int32_t> numbers;

            if (flag == "--db") {
                options.mDb = value;
            } else if (flag == "--behavior-packs") {
                options.mBehaviorPacks = value;
            } else if (flag == "--seed") {
                if (!parseInt64(value, options.mSeed)) {
                    LOG_ERROR(LogAreaID::Level, "--seed must be a 64-bit integer, got '%s'", value.c_str());
                    return false;
                }
                options.mHasSeed = true;
            } else if (flag == "--area") {
                if (!parseInt32List(value, 4, numbers)) {
                    LOG_ERROR(LogAreaID::Level, "--area must be x0,z0,x1,z1 in chunk coordinates, got '%s'",
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
                    LOG_ERROR(LogAreaID::Level, "--column must be x,z in block coordinates, got '%s'", value.c_str());
                    return false;
                }
                options.mColumnX = numbers[0];
                options.mColumnZ = numbers[1];
                options.mHasColumn = true;
            } else if (flag == "--dimension") {
                if (value != "0" && value != "1" && value != "2") {
                    LOG_ERROR(LogAreaID::Level, "--dimension must be 0, 1 or 2, got '%s'", value.c_str());
                    return false;
                }
                selectDimension(Dimension::fromId(value[0] - '0'));
            } else if (flag == "--mineshaft-markers") {
                if (!parseInt32List(value, 2, numbers) || numbers[0] < LevelChunk::MIN_Y
                    || numbers[1] > LevelChunk::MAX_Y || numbers[0] > numbers[1]) {
                    LOG_ERROR(LogAreaID::Level, "--mineshaft-markers needs ascending Overworld block heights minY,maxY");
                    return false;
                }
                options.mMineshaftMarkers = true;
                options.mMineshaftMinY = numbers[0];
                options.mMineshaftMaxY = numbers[1];
            } else if (flag == "--csv") {
                options.mCsv = value;
            } else if (flag == "--json") {
                options.mJson = value;
            } else if (flag == "--blocks") {
                options.mBlocks = value;
            } else if (flag == "--order") {
                options.mOrder = value;
            } else if (flag == "--threads") {
                options.mThreads = (size_t) std::max(1, std::atoi(value.c_str()));
            } else if (flag == "--dump") {
                options.mDump = value;
            } else if (flag == "--min-block-match" || flag == "--min-mineshaft-marker-match") {
                char *end = nullptr;
                const double threshold = std::strtod(value.c_str(), &end);
                if (value.empty() || *end != '\0' || !(threshold >= 0.0) || threshold > 100.0) {
                    LOG_ERROR(LogAreaID::Level, "%s must be a percentage, got '%s'", flag.c_str(), value.c_str());
                    return false;
                }
                if (flag == "--min-block-match") {
                    options.mMinBlockMatch = threshold;
                    options.mHasMinimum = true;
                } else {
                    options.mMinMineshaftMatch = threshold;
                    options.mHasMineshaftMinimum = true;
                }
            } else {
                LOG_ERROR(LogAreaID::Level, "unknown option %s", flag.c_str());
                return false;
            }
        }

        if (options.mDb.empty() || !options.mHasSeed) {
            LOG_ERROR(LogAreaID::Level, "--db and --seed are required");
            return false;
        }

        if ((int) options.mHasArea + (int) options.mHasColumn + (int) options.mFar != 1) {
            LOG_ERROR(LogAreaID::Level, "give exactly one of --area, --column and --far");
            return false;
        }

        if (options.mHasMineshaftMinimum && !options.mMineshaftMarkers) {
            LOG_ERROR(LogAreaID::Level, "--min-mineshaft-marker-match requires --mineshaft-markers");
            return false;
        }
        if (options.mMineshaftMarkers && (!options.mHasArea || options.mTerrainOnly
                                          || gDimension != DimensionType::Overworld)) {
            LOG_ERROR(LogAreaID::Level, "--mineshaft-markers requires a populated Overworld --area");
            return false;
        }

        if (options.mFar && (!options.mCsv.empty() || !options.mJson.empty() || !options.mDump.empty())) {
            LOG_ERROR(LogAreaID::Level, "--far prints one report per region and cannot write a file");
            return false;
        }

        if (options.mHasArea && options.mArea.count() > MAX_AREA_CHUNKS) {
            LOG_ERROR(LogAreaID::Level, "the area covers %lld chunks, the limit is %lld",
                                        (long long) options.mArea.count(), (long long) MAX_AREA_CHUNKS);
            return false;
        }

        return true;
    }

    // LevelDB's recovery and lock files must never touch the original BDS world
    class WorldCopy {
    public:
        const std::filesystem::path &root() const { return mRoot; }
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

        std::string getWorldsDirectory() const {
            return mRoot.string();
        }

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

            // the rock the noise fills these dimensions with: left out, their whole terrain would count as features
            if (gDimension == DimensionType::Nether)
                mBaseTerrain.insert(VanillaBlocks::NETHERRACK().getIdentifier());
            if (gDimension == DimensionType::TheEnd)
                mBaseTerrain.insert(VanillaBlocks::END_STONE().getIdentifier());

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
                                                 "minecraft:raw_iron_block", "minecraft:raw_copper_block",
                                                 "minecraft:netherrack", "minecraft:end_stone"};
            for (const char *solid: SOLID) {
                if (name == solid)
                    return 2;
            }

            return name.size() > 4 && name.compare(name.size() - 4, 4, "_ore") == 0 ? 2 : -1;
        }

        // a block one of the underground ore rules places
        bool isOrePlaced(const BlockState &state) const {
            const std::string &name = state.mName;
            static const char *const PLACED[] = {"minecraft:granite", "minecraft:diorite", "minecraft:andesite",
                                                 "minecraft:tuff", "minecraft:dirt", "minecraft:gravel",
                                                 "minecraft:clay", "minecraft:infested_stone",
                                                 "minecraft:infested_deepslate"};
            for (const char *placed: PLACED) {
                if (name == placed)
                    return true;
            }

            return name.size() > 4 && name.compare(name.size() - 4, 4, "_ore") == 0;
        }

        // a mismatch an ore blob explains: a placed block against another one or against its host rock
        bool isOreSpill(const BlockState &expected, const BlockState &generated) const {
            const auto hostOrPlaced = [this](const BlockState &state) {
                return state.mName == "minecraft:stone" || state.mName == "minecraft:deepslate" || isOrePlaced(state);
            };
            return (isOrePlaced(expected) || isOrePlaced(generated)) && hostOrPlaced(expected)
                   && hostOrPlaced(generated);
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

    // the game saves no biomes above the highest non-empty section, repeating it instead:
    // generated biomes there are read from that section, whatever the generated blocks
    uint32_t savedBiomeAt(const LevelChunk &generated, const LevelChunk &expected, int x, int32_t y, int z) {
        const int32_t savedSections = std::max(1, expected.getNetworkSubChunkCount());
        const int32_t section = (y - gMinY) >> 4;
        if (section < savedSections)
            return generated.getBiomeAt(x, y, z);

        return generated.getBiomeAt(x, gMinY + ((savedSections - 1) << 4) + ((y - gMinY) & 15), z);
    }

    int32_t surfaceHeight(const LevelChunk &chunk, int x, int z, const BlockClassifier &classifier) {
        for (int32_t y = gMaxY; y >= gMinY; y--) {
            if (!classifier.isAir(chunk.getBlock(x, y, z)))
                return y;
        }

        return gMinY - 1;
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

    std::string gOrder = "rows";
    size_t gThreads = 1;

    // the area's chunks in a request order: rows, spiral from the centre, reverse (of the spiral), random:<seed>
    std::vector<std::pair<int32_t, int32_t>> requestOrder(const ChunkArea &area, const std::string &order) {
        std::vector<std::pair<int32_t, int32_t>> chunks;
        for (int32_t chunkZ = area.mZ0; chunkZ <= area.mZ1; chunkZ++) {
            for (int32_t chunkX = area.mX0; chunkX <= area.mX1; chunkX++)
                chunks.emplace_back(chunkX, chunkZ);
        }
        if (order == "rows")
            return chunks;

        const double centreX = (area.mX0 + area.mX1) / 2.0;
        const double centreZ = (area.mZ0 + area.mZ1) / 2.0;
        std::stable_sort(chunks.begin(), chunks.end(), [&](const auto &a, const auto &b) {
            const double ringA = std::max(std::abs(a.first - centreX), std::abs(a.second - centreZ));
            const double ringB = std::max(std::abs(b.first - centreX), std::abs(b.second - centreZ));
            if (ringA != ringB)
                return ringA < ringB;
            return std::atan2(a.second - centreZ, a.first - centreX) < std::atan2(b.second - centreZ, b.first - centreX);
        });
        if (order == "reverse") {
            std::reverse(chunks.begin(), chunks.end());
        } else if (order.rfind("random:", 0) == 0) {
            std::mt19937 random((uint32_t) std::strtoul(order.c_str() + 7, nullptr, 10));
            std::shuffle(chunks.begin(), chunks.end(), random);
        }
        return chunks;
    }

    // generated as the workers do, in the --order requests on --threads workers, visited in rows
    void generateArea(int64_t seed, const ChunkArea &area, bool terrainOnly,
                      const std::function<void(LevelChunk &)> &visit) {
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(gDimension,
                                                                                         seed);
        TerrainCache terrainCache(*generator);
        const std::vector<std::pair<int32_t, int32_t>> order = requestOrder(area, gOrder);
        std::unordered_map<int64_t, std::unique_ptr<LevelChunk>> generated;
        std::mutex mutex;
        std::atomic<size_t> next(0);
        std::vector<std::thread> workers;
        // built on this thread: a generator's constructor sets shared noise, one at a time
        std::vector<std::unique_ptr<GeneratorChunkSource>> sources;
        for (size_t i = 0; i < std::max<size_t>(1, gThreads); i++)
            sources.push_back(std::unique_ptr<GeneratorChunkSource>(new GeneratorChunkSource(seed, gDimension, terrainCache)));
        for (size_t i = 0; i < std::max<size_t>(1, gThreads); i++) {
            workers.emplace_back([&, i]() {
                GeneratorChunkSource &source = *sources[i];
                for (size_t index = next++; index < order.size(); index = next++) {
                    const int32_t chunkX = order[index].first;
                    const int32_t chunkZ = order[index].second;
                    std::vector<Tag> blockActors;
                    std::vector<Tag> entities;
                    std::unique_ptr<LevelChunk> chunk(new LevelChunk(
                            terrainOnly ? terrainCache.terrain(chunkX, chunkZ)
                                        : source.generate(chunkX, chunkZ, blockActors, entities)));
                    std::lock_guard<std::mutex> lock(mutex);
                    generated[packChunk(chunkX, chunkZ)] = std::move(chunk);
                }
            });
        }
        for (std::thread &worker: workers)
            worker.join();

        for (int32_t chunkZ = area.mZ0; chunkZ <= area.mZ1; chunkZ++) {
            for (int32_t chunkX = area.mX0; chunkX <= area.mX1; chunkX++)
                visit(*generated[packChunk(chunkX, chunkZ)]);
        }
    }

    enum class LoadResult {
        Loaded,
        Missing,
        NotFinalized,
        Unreadable
    };

    LoadResult loadExpected(LevelStorage &storage, LevelChunk &chunk) {
        chunk.setDimension(gDimension);

        try {
            if (!storage.loadChunk(chunk))
                return LoadResult::Missing;
        } catch (const std::exception &exception) {
            LOG_WARN(LogAreaID::Level, "chunk %d %d is unreadable: %s", chunk.getX(), chunk.getZ(), exception.what());
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
                : mClassifier(classifier), mBiomeNames(biomes), mBands((size_t) yBandCount()),
                  mHeightHistogram(HEIGHT_BUCKETS, 0) {}

        ChunkScore compare(const LevelChunk &expected, const LevelChunk &generated) {
            ChunkScore score;
            score.mX = expected.getX();
            score.mZ = expected.getZ();

            for (int x = 0; x < 16; x++) {
                for (int z = 0; z < 16; z++) {
                    for (int32_t y = gMinY; y <= gMaxY; y++)
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
                    for (int32_t y = gMinY; y <= gMaxY; y += BIOME_CELL_SIZE) {
                        const uint32_t expectedBiome = expected.getBiomeAt(x, y, z);
                        const uint32_t generatedBiome = savedBiomeAt(generated, expected, x, y, z);
                        score.mBiomes.add(expectedBiome == generatedBiome);
                        mBiomesByExpected[expectedBiome].add(expectedBiome == generatedBiome);
                        if (expectedBiome != generatedBiome) {
                            const std::pair<std::string, std::string> pair{mBiomeNames.get(expectedBiome),
                                                                           mBiomeNames.get(generatedBiome)};
                            if (mBiomeMismatches[pair]++ < 3)
                                mBiomeSamples[pair].push_back({expected.getX() * 16 + x, y, expected.getZ() * 16 + z});
                        }
                    }
                }
            }

            _merge(mBlocks, score.mBlocks);
            _merge(mHeights, score.mHeights);
            _merge(mBiomes, score.mBiomes);
            mChunks.push_back(score);
            return score;
        }

        const Ratio &getBlocks() const {
            return mBlocks;
        }

        void printText(int64_t seed, const ChunkArea &area, const uint64_t *skipped) const {
            LOG_INFO(LogAreaID::Level, "World generation parity, seed %lld, chunks %d,%d to %d,%d", (long long) seed,
                                       area.mX0, area.mZ0, area.mX1, area.mZ1);
            LOG_INFO(LogAreaID::Level, "Chunks: %lld requested, %zu compared, %llu never saved by BDS, %llu not "
                                       "finalized, %llu unreadable", (long long) area.count(), mChunks.size(),
                                       (unsigned long long) skipped[0], (unsigned long long) skipped[1],
                                       (unsigned long long) skipped[2]);

            LOG_INFO(LogAreaID::Level, "Blocks: %.3f%% identical, %.3f%% same class (air/fluid/solid) over %llu blocks",
                                       mBlocks.percent(), mCoarse.percent(), (unsigned long long) mBlocks.mTotal);
            LOG_INFO(LogAreaID::Level, "Blocks outside ore spills: %.3f%% identical, %llu ore layer mismatches set "
                                       "aside", _outsideOreSpills(), (unsigned long long) mOreSpills);
            LOG_INFO(LogAreaID::Level, "Surface height: %.3f%% equal, mean |diff| %.3f, max |diff| %lld over %llu "
                                       "columns", mHeights.percent(), _meanHeightDifference(),
                                       (long long) mMaxHeightDifference, (unsigned long long) mHeights.mTotal);
            LOG_INFO(LogAreaID::Level, "  |diff|  columns");
            for (size_t bucket = 0; bucket < HEIGHT_BUCKETS; bucket++)
                LOG_INFO(LogAreaID::Level, "  %-6s  %llu", HEIGHT_BUCKET_NAMES[bucket],
                                           (unsigned long long) mHeightHistogram[bucket]);
            LOG_INFO(LogAreaID::Level, "Biomes: %.3f%% equal over %llu 4x4x4 cells", mBiomes.percent(),
                                       (unsigned long long) mBiomes.mTotal);

            static const char *const SHAPES[3] = {"open", "fluid", "solid"};
            LOG_INFO(LogAreaID::Level, "Terrain shape: %.3f%% same (open/fluid/solid) over %llu blocks",
                                       mShape.percent(), (unsigned long long) mShape.mTotal);
            std::vector<std::pair<std::pair<int, int>, ShapeMismatch>> shapes(mShapeMismatches.begin(),
                                                                              mShapeMismatches.end());
            std::sort(shapes.begin(), shapes.end(), [](const std::pair<std::pair<int, int>, ShapeMismatch> &a,
                                                       const std::pair<std::pair<int, int>, ShapeMismatch> &b) {
                return a.second.mCount > b.second.mCount;
            });
            for (size_t i = 0; i < shapes.size() && i < 16; i++) {
                const int pair = shapes[i].first.first;
                const int32_t minY = gMinY + shapes[i].first.second * Y_BAND_HEIGHT;
                std::string line = formatted("  %10llu  %-5s -> %-5s y %4d..%-4d",
                                             (unsigned long long) shapes[i].second.mCount, SHAPES[pair / 3],
                                             SHAPES[pair % 3], minY, minY + Y_BAND_HEIGHT - 1);
                for (const std::array<int32_t, 3> &sample: shapes[i].second.mSamples)
                    line += formatted("  (%d %d %d)", sample[0], sample[1], sample[2]);
                LOG_INFO(LogAreaID::Level, "%s", line.c_str());
            }

            LOG_INFO(LogAreaID::Level, "By category        blocks       match");
            for (int category = 0; category < CATEGORY_COUNT; category++)
                LOG_INFO(LogAreaID::Level, "  %-14s %12llu  %8.3f%%", CATEGORY_KEYS[category],
                                           (unsigned long long) mCategories[category].mTotal,
                                           mCategories[category].percent());

            LOG_INFO(LogAreaID::Level, "By Y band          blocks       match");
            for (size_t band = 0; band < mBands.size(); band++) {
                const int32_t minY = gMinY + (int32_t) band * Y_BAND_HEIGHT;
                LOG_INFO(LogAreaID::Level, "  %4d..%-4d    %12llu  %8.3f%%", minY, minY + Y_BAND_HEIGHT - 1,
                                           (unsigned long long) mBands[band].mTotal, mBands[band].percent());
            }

            LOG_INFO(LogAreaID::Level, "Top block mismatches (expected -> generated)");
            for (const std::pair<std::pair<std::string, std::string>, uint64_t> &entry: _top(mBlockMismatches))
                LOG_INFO(LogAreaID::Level, "  %10llu  %s -> %s", (unsigned long long) entry.second,
                                           entry.first.first.c_str(), entry.first.second.c_str());

            LOG_INFO(LogAreaID::Level, "Biome accuracy by expected biome");
            std::vector<std::pair<uint32_t, Ratio>> byBiome(mBiomesByExpected.begin(), mBiomesByExpected.end());
            std::sort(byBiome.begin(), byBiome.end(),
                      [](const std::pair<uint32_t, Ratio> &a, const std::pair<uint32_t, Ratio> &b) {
                          return a.second.mTotal > b.second.mTotal;
                      });
            for (const std::pair<uint32_t, Ratio> &entry: byBiome)
                LOG_INFO(LogAreaID::Level, "  %-28s %10llu cells  %8.3f%%", mBiomeNames.get(entry.first).c_str(),
                                           (unsigned long long) entry.second.mTotal, entry.second.percent());

            LOG_INFO(LogAreaID::Level, "Top biome confusions (expected -> generated)");
            for (const std::pair<std::pair<std::string, std::string>, uint64_t> &entry: _top(mBiomeMismatches)) {
                std::string line = formatted("  %10llu  %s -> %s", (unsigned long long) entry.second,
                                             entry.first.first.c_str(), entry.first.second.c_str());
                const auto samples = mBiomeSamples.find(entry.first);
                if (samples != mBiomeSamples.end()) {
                    for (const std::array<int32_t, 3> &sample: samples->second)
                        line += formatted("  (%d %d %d)", sample[0], sample[1], sample[2]);
                }
                LOG_INFO(LogAreaID::Level, "%s", line.c_str());
            }
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
                          "  \"blocks\": {\"total\": %llu, \"match_pct\": %.4f, \"coarse_match_pct\": %.4f, "
                          "\"ore_spills\": %llu, \"outside_ore_spills_match_pct\": %.4f},\n",
                          (unsigned long long) mBlocks.mTotal, mBlocks.percent(), mCoarse.percent(),
                          (unsigned long long) mOreSpills, _outsideOreSpills());
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
                const int32_t minY = gMinY + (int32_t) band * Y_BAND_HEIGHT;
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
                                                            (y - gMinY) / Y_BAND_HEIGHT}];
                if (mismatch.mCount++ < 3)
                    mismatch.mSamples.push_back({x, y, z});
            }
            score.mBlocks.add(matched);
            if (!matched && mClassifier.isOreSpill(expected, generated))
                mOreSpills++;
            mCoarse.add(mClassifier.coarse(expected) == mClassifier.coarse(generated));
            mCategories[std::max(mClassifier.category(expected), mClassifier.category(generated))].add(matched);
            mBands[(size_t) ((y - gMinY) / Y_BAND_HEIGHT)].add(matched);

            if (matched)
                return;

            if (expected.mName == generated.mName) {
                mBlockMismatches[{describeState(expected), describeState(generated)}]++;
                return;
            }
            mBlockMismatches[{expected.mName, generated.mName}]++;
        }

        double _outsideOreSpills() const {
            const uint64_t total = mBlocks.mTotal - mOreSpills;
            return total == 0 ? 0.0 : 100.0 * (double) mBlocks.mMatched / (double) total;
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
        uint64_t mOreSpills = 0;
        Ratio mCoarse;
        Ratio mShape;

        struct ShapeMismatch {
            uint64_t mCount = 0;
            std::vector<std::array<int32_t, 3>> mSamples;
        };

        std::map<std::pair<int, int>, ShapeMismatch> mShapeMismatches;
        std::map<std::pair<std::string, std::string>, std::vector<std::array<int32_t, 3>>> mBiomeSamples;
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
            LOG_ERROR(LogAreaID::Level, "chunk %d %d holding column %d,%d is %s in the BDS world", chunkX, chunkZ,
                                        options.mColumnX, options.mColumnZ,
                                        loaded == LoadResult::Missing ? "absent" : "unreadable");
            return EXIT_INVALID;
        }
        if (loaded == LoadResult::NotFinalized)
            LOG_INFO(LogAreaID::Level, "BDS has not finished populating chunk %d %d", chunkX, chunkZ);

        ChunkArea area;
        area.mX0 = area.mX1 = chunkX;
        area.mZ0 = area.mZ1 = chunkZ;
        const int x = options.mColumnX & 15;
        const int z = options.mColumnZ & 15;

        generateArea(options.mSeed, area, options.mTerrainOnly, [&](LevelChunk &generated) {
            LOG_INFO(LogAreaID::Level, "Column %d,%d (chunk %d %d), seed %lld; '*' marks a difference",
                                       options.mColumnX, options.mColumnZ, chunkX, chunkZ, (long long) options.mSeed);
            for (int32_t y = gMaxY; y >= gMinY; y--) {
                const BlockState &expectedBlock = expected.getBlock(x, y, z);
                const BlockState &generatedBlock = generated.getBlock(x, y, z);
                const uint32_t expectedBiome = expected.getBiomeAt(x, y, z);
                const uint32_t generatedBiome = savedBiomeAt(generated, expected, x, y, z);
                LOG_INFO(LogAreaID::Level, "%c%c %4d  %-40s %-40s %-22s %s",
                                           expectedBlock == generatedBlock ? ' ' : '*',
                                           expectedBiome == generatedBiome ? ' ' : '*', y,
                                           describeState(expectedBlock).c_str(), describeState(generatedBlock).c_str(),
                                           biomes.get(expectedBiome).c_str(), biomes.get(generatedBiome).c_str());
            }
        });

        return 0;
    }

    // one line per block that differs or that is not plain terrain in either world: x y z expected generated
    void writeBlocks(std::ofstream &out, const LevelChunk &expected, const LevelChunk &generated, bool states) {
        static const std::unordered_set<std::string> TERRAIN = {
                "minecraft:air", "minecraft:stone", "minecraft:deepslate", "minecraft:water", "minecraft:bedrock",
                "minecraft:lava", "minecraft:dirt", "minecraft:grass_block"};
        for (int32_t x = 0; x < 16; x++) {
            for (int32_t z = 0; z < 16; z++) {
                for (int32_t y = gMinY; y <= gMaxY; y++) {
                    const BlockState &expectedBlock = expected.getBlock(x, y, z);
                    const BlockState &generatedBlock = generated.getBlock(x, y, z);
                    if (expectedBlock == generatedBlock && TERRAIN.count(expectedBlock.mName) != 0)
                        continue;

                    out << ((expected.getX() << 4) + x) << ' ' << y << ' ' << ((expected.getZ() << 4) + z) << ' '
                        << expectedBlock.mName << ' ' << generatedBlock.mName;
                    if (states)
                        out << '\t' << expectedBlock.mStates.toString() << '\t' << generatedBlock.mStates.toString();
                    out << '\n';
                }
            }
        }
    }

    struct PlacementScore {
        const char *mName;
        uint64_t mExpected = 0;
        uint64_t mGenerated = 0;
        uint64_t mShared = 0;
        uint64_t mGeneratedWithoutReference = 0;
        uint64_t mMarkerPositions = 0;
        uint64_t mMatchingMarkers = 0;
        uint64_t mSharedMarkerPositions = 0;

        void add(bool expected, bool generated) {
            mExpected += expected;
            mGenerated += generated;
            mShared += expected && generated;
        }

        void addMarker(bool expected, bool generated, bool sameState) {
            mMarkerPositions += expected || generated;
            mSharedMarkerPositions += expected && generated;
            mMatchingMarkers += expected && generated && sameState;
        }

        void print() const {
            if (mGeneratedWithoutReference != 0)
                LOG_INFO(LogAreaID::Level, "Nether placement %s: %llu Falcon chunks lack a BDS reference",
                         mName, (unsigned long long) mGeneratedWithoutReference);

            const uint64_t total = mExpected + mGenerated - mShared;
            if (total == 0) {
                LOG_INFO(LogAreaID::Level, "Nether placement %s: no positive chunks in either world", mName);
                return;
            }
            const double percent = 100.0 * (double) mShared / (double) total;
            LOG_INFO(LogAreaID::Level, "Nether placement %s: %.2f%% overlap (%llu shared, %llu BDS, %llu Falcon)",
                     mName, percent, (unsigned long long) mShared, (unsigned long long) mExpected,
                     (unsigned long long) mGenerated);
            if (mMarkerPositions != 0)
                LOG_INFO(LogAreaID::Level, "Nether markers %s: %.2f%% identical (%llu / %llu; %llu shared positions before comparing states)",
                         mName, 100.0 * (double) mMatchingMarkers / (double) mMarkerPositions,
                         (unsigned long long) mMatchingMarkers, (unsigned long long) mMarkerPositions,
                         (unsigned long long) mSharedMarkerPositions);
        }
    };

    struct NetherPlacement {
        bool mFossil = false;
        bool mPortal = false;
        bool mBastion = false;
        bool mDriedGhast = false;

        void merge(const NetherPlacement &other) {
            mFossil |= other.mFossil;
            mPortal |= other.mPortal;
            mBastion |= other.mBastion;
            mDriedGhast |= other.mDriedGhast;
        }
    };

    NetherPlacement netherMarkers(const BlockState &block) {
        const std::string &name = block.mName;
        return {name == "minecraft:bone_block",
                name == "minecraft:obsidian" || name == "minecraft:crying_obsidian",
                name == "minecraft:polished_blackstone_bricks"
                        || name == "minecraft:cracked_polished_blackstone_bricks"
                        || name == "minecraft:gilded_blackstone",
                name == "minecraft:dried_ghast"};
    }

    NetherPlacement netherBounds(const LevelChunk &chunk) {
        const ChunkStructureBounds &bounds = chunk.getStructureBounds();
        return {bounds.hasName("minecraft:nether_fossil"), bounds.hasName("minecraft:ruined_portal"),
                bounds.hasName("minecraft:bastion_remnant")};
    }

    void compareNetherPlacement(const LevelChunk *expected, const LevelChunk &generated, PlacementScore &fossils,
                                PlacementScore &portals, PlacementScore &bastions, PlacementScore &driedGhasts) {
        NetherPlacement falcon = netherBounds(generated);
        NetherPlacement vanilla = expected != nullptr ? netherBounds(*expected) : NetherPlacement{};
        // fossils and older saves can lack bounds: use the generated blocks as placement markers
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t z = 0; z < 16; ++z) {
                for (int32_t y = gMinY; y <= gMaxY; ++y) {
                    const BlockState &actual = generated.getBlock(x, y, z);
                    const NetherPlacement actualMarkers = netherMarkers(actual);
                    falcon.merge(actualMarkers);
                    if (expected == nullptr)
                        continue;

                    const BlockState &reference = expected->getBlock(x, y, z);
                    const NetherPlacement referenceMarkers = netherMarkers(reference);
                    vanilla.merge(referenceMarkers);
                    const bool sameState = reference == actual;
                    fossils.addMarker(referenceMarkers.mFossil, actualMarkers.mFossil, sameState);
                    portals.addMarker(referenceMarkers.mPortal, actualMarkers.mPortal, sameState);
                    bastions.addMarker(referenceMarkers.mBastion, actualMarkers.mBastion, sameState);
                    driedGhasts.addMarker(referenceMarkers.mDriedGhast, actualMarkers.mDriedGhast, sameState);
                }
            }
        }

        if (expected == nullptr) {
            fossils.mGeneratedWithoutReference += falcon.mFossil;
            portals.mGeneratedWithoutReference += falcon.mPortal;
            bastions.mGeneratedWithoutReference += falcon.mBastion;
            driedGhasts.mGeneratedWithoutReference += falcon.mDriedGhast;
            return;
        }

        fossils.add(vanilla.mFossil, falcon.mFossil);
        portals.add(vanilla.mPortal, falcon.mPortal);
        bastions.add(vanilla.mBastion, falcon.mBastion);
        driedGhasts.add(vanilla.mDriedGhast, falcon.mDriedGhast);
    }

    bool isMineshaftMarker(const BlockState &block) {
        const std::string &name = block.mName;
        return name == "minecraft:oak_planks" || name == "minecraft:oak_fence"
               || name == "minecraft:dark_oak_planks" || name == "minecraft:dark_oak_fence"
               || name == "minecraft:rail" || name == "minecraft:web"
               || name == "minecraft:torch" || name == "minecraft:mob_spawner"
               || name == "minecraft:iron_chain" || name == "minecraft:oak_log"
               || name == "minecraft:dark_oak_log";
    }

    void compareMineshaftMarkers(const LevelChunk &expected, const LevelChunk &generated, const Options &options,
                                  PlacementScore &score) {
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t z = 0; z < 16; ++z) {
                for (int32_t y = options.mMineshaftMinY; y <= options.mMineshaftMaxY; ++y) {
                    const BlockState &reference = expected.getBlock(x, y, z);
                    const BlockState &actual = generated.getBlock(x, y, z);
                    score.addMarker(isMineshaftMarker(reference), isMineshaftMarker(actual), reference == actual);
                }
            }
        }
    }

    class ChunkDump {
    public:
        bool open(const std::string &path, const BiomeNames &biomes) {
            mPath = path;
            mBiomes = &biomes;
            mOut.open(path, std::ios::binary);
            if (!mOut)
                return false;

            mOut.write("FWGD", 4);
            _put<int32_t>(1);
            _put<int32_t>(Dimension::toId(gDimension));
            _put<int32_t>(gMinY);
            _put<int32_t>(gMaxY);
            return true;
        }

        bool isOpen() const {
            return mOut.is_open();
        }

        void write(int32_t chunkX, int32_t chunkZ, uint8_t loadResult, const LevelChunk *expected,
                   const LevelChunk &generated) {
            _put<int32_t>(chunkX);
            _put<int32_t>(chunkZ);
            _put<uint8_t>(loadResult);
            if (expected == nullptr)
                return;

            _writeChunk(*expected, *expected);
            _writeChunk(generated, *expected);
        }

        bool close() {
            mOut.close();
            std::ofstream strings(mPath + ".strings");
            for (const std::string &text: mStrings)
                strings << text << '\n';
            return (bool) strings;
        }

    private:
        template<typename T>
        void _put(T value) {
            mOut.write(reinterpret_cast<const char *>(&value), sizeof(T));
        }

        uint16_t _index(const std::string &text) {
            const auto found = mIndices.find(text);
            if (found != mIndices.end())
                return found->second;

            const uint16_t index = (uint16_t) mStrings.size();
            mStrings.push_back(text);
            mIndices.emplace(text, index);
            return index;
        }

        // the chunk's palette entries keep their address while it lives: one string per entry
        void _writeChunk(const LevelChunk &chunk, const LevelChunk &expected) {
            std::unordered_map<const BlockState *, uint16_t> cache;
            std::vector<uint16_t> values;
            values.reserve((size_t) 256 * (gMaxY - gMinY + 1));
            for (const int layer: {0, 1}) {
                values.clear();
                for (int x = 0; x < 16; x++) {
                    for (int z = 0; z < 16; z++) {
                        for (int32_t y = gMinY; y <= gMaxY; y++) {
                            const BlockState &state = chunk.getBlock(x, y, z, layer);
                            const auto found = cache.find(&state);
                            if (found != cache.end()) {
                                values.push_back(found->second);
                                continue;
                            }
                            const uint16_t index = _index(describeState(state));
                            cache.emplace(&state, index);
                            values.push_back(index);
                        }
                    }
                }
                mOut.write(reinterpret_cast<const char *>(values.data()), (std::streamsize) (values.size() * 2));
            }

            values.clear();
            for (int x = 0; x < 16; x += BIOME_CELL_SIZE) {
                for (int z = 0; z < 16; z += BIOME_CELL_SIZE) {
                    for (int32_t y = gMinY; y <= gMaxY; y += BIOME_CELL_SIZE) {
                        const uint32_t biome = &chunk == &expected ? chunk.getBiomeAt(x, y, z)
                                                                   : savedBiomeAt(chunk, expected, x, y, z);
                        values.push_back(_index(mBiomes->get(biome)));
                    }
                }
            }
            mOut.write(reinterpret_cast<const char *>(values.data()), (std::streamsize) (values.size() * 2));

            const ChunkStructureBounds &bounds = chunk.getStructureBounds();
            std::unordered_map<uint32_t, std::string> names;
            for (const ChunkStructureBounds::Name &name: bounds.getNames())
                names[name.mEntity] = name.mName;
            _put<uint32_t>((uint32_t) bounds.getEntries().size());
            for (const ChunkStructureBounds::Entry &entry: bounds.getEntries()) {
                const auto name = names.find(entry.mName);
                _put<uint16_t>(_index(name == names.end() ? "?" : name->second));
                for (const int32_t value: {entry.mBox.minX, entry.mBox.minY, entry.mBox.minZ, entry.mBox.maxX,
                                           entry.mBox.maxY, entry.mBox.maxZ})
                    _put<int32_t>(value);
            }
        }

        std::string mPath;
        const BiomeNames *mBiomes = nullptr;
        std::ofstream mOut;
        std::vector<std::string> mStrings;
        std::unordered_map<std::string, uint16_t> mIndices;
    };

    int compareArea(LevelStorage &storage, const Options &options, const ChunkArea &area,
                    const BlockClassifier &classifier, const BiomeNames &biomes) {
        ParityReport report(classifier, biomes);
        const bool trackPlacement = gDimension == DimensionType::Nether && !options.mTerrainOnly;
        PlacementScore fossils{"nether_fossil"};
        PlacementScore portals{"ruined_portal"};
        PlacementScore bastions{"bastion_remnant"};
        PlacementScore driedGhasts{"dried_ghast"};
        PlacementScore mineshaft{"mineshaft"};
        uint64_t skipped[3] = {0, 0, 0};
        std::ofstream blocks;
        if (!options.mBlocks.empty())
            blocks.open(options.mBlocks);
        ChunkDump dump;
        if (!options.mDump.empty() && !dump.open(options.mDump, biomes)) {
            LOG_ERROR(LogAreaID::Level, "could not write %s", options.mDump.c_str());
            return EXIT_INVALID;
        }

        generateArea(options.mSeed, area, options.mTerrainOnly, [&](LevelChunk &generated) {
            LevelChunk expected(generated.getX(), generated.getZ());
            const LoadResult loaded = loadExpected(storage, expected);
            if (dump.isOpen())
                dump.write(generated.getX(), generated.getZ(), (uint8_t) loaded,
                           loaded == LoadResult::Loaded ? &expected : nullptr, generated);
            if (loaded != LoadResult::Loaded) {
                skipped[(int) loaded - 1]++;
                if (trackPlacement)
                    compareNetherPlacement(nullptr, generated, fossils, portals, bastions, driedGhasts);
                return;
            }

            report.compare(expected, generated);
            if (options.mMineshaftMarkers)
                compareMineshaftMarkers(expected, generated, options, mineshaft);
            if (trackPlacement)
                compareNetherPlacement(&expected, generated, fossils, portals, bastions, driedGhasts);
            if (blocks.is_open())
                writeBlocks(blocks, expected, generated, options.mDumpBlockStates);
        });

        if (dump.isOpen() && !dump.close()) {
            LOG_ERROR(LogAreaID::Level, "could not write %s.strings", options.mDump.c_str());
            return EXIT_INVALID;
        }
        if (report.getBlocks().mTotal == 0) {
            LOG_ERROR(LogAreaID::Level, "BDS saved no finalized chunk in this area");
            return EXIT_INVALID;
        }

        report.printText(options.mSeed, area, skipped);
        double mineshaftScore = 0.0;
        if (options.mMineshaftMarkers) {
            if (mineshaft.mMarkerPositions == 0) {
                LOG_ERROR(LogAreaID::Level, "No mineshaft markers in the compared chunks and height range");
                return EXIT_INVALID;
            }
            mineshaftScore = 100.0 * (double) mineshaft.mMatchingMarkers / (double) mineshaft.mMarkerPositions;
            LOG_INFO(LogAreaID::Level,
                     "Mineshaft markers Y %d..%d: %.3f%% identical (%llu / %llu; %llu shared positions)",
                     options.mMineshaftMinY, options.mMineshaftMaxY, mineshaftScore,
                     (unsigned long long) mineshaft.mMatchingMarkers,
                     (unsigned long long) mineshaft.mMarkerPositions,
                     (unsigned long long) mineshaft.mSharedMarkerPositions);
        }
        if (trackPlacement) {
            fossils.print();
            portals.print();
            bastions.print();
            driedGhasts.print();
        }

        if (!options.mCsv.empty() && !report.writeCsv(options.mCsv)) {
            LOG_ERROR(LogAreaID::Level, "could not write %s", options.mCsv.c_str());
            return EXIT_INVALID;
        }

        if (!options.mJson.empty() && !report.writeJson(options.mJson, options.mSeed, area, skipped)) {
            LOG_ERROR(LogAreaID::Level, "could not write %s", options.mJson.c_str());
            return EXIT_INVALID;
        }

        if (options.mHasMineshaftMinimum && mineshaftScore < options.mMinMineshaftMatch) {
            LOG_ERROR(LogAreaID::Level, "mineshaft marker match %.3f%% is below the required %.3f%%",
                      mineshaftScore, options.mMinMineshaftMatch);
            return EXIT_BELOW_THRESHOLD;
        }
        if (options.mHasMineshaftMinimum && (skipped[0] || skipped[1] || skipped[2])) {
            LOG_ERROR(LogAreaID::Level, "mineshaft marker threshold requires a finalized BDS reference for every chunk");
            return EXIT_INVALID;
        }
        if (options.mHasMinimum && report.getBlocks().percent() < options.mMinBlockMatch) {
            LOG_ERROR(LogAreaID::Level, "block match %.3f%% is below the required %.3f%%", report.getBlocks().percent(),
                                        options.mMinBlockMatch);
            return EXIT_BELOW_THRESHOLD;
        }

        return 0;
    }

    int run(const Options &options) {
        WorldCopy copy;
        std::string message;
        if (!copy.create(options.mDb, message)) {
            LOG_ERROR(LogAreaID::Level, "%s", message.c_str());
            return EXIT_INVALID;
        }

        LevelStorage storage;
        if (!storage.open(copy.getWorldsDirectory(), WorldCopy::LEVEL_NAME, Dimension::toId(gDimension))) {
            LOG_ERROR(LogAreaID::Level, "could not open '%s' as a LevelDB database", options.mDb.c_str());
            return EXIT_INVALID;
        }

        Tag levelDat;
        const bool hasReferenceSeed = storage.readLevelDat(levelDat) && levelDat.contains("RandomSeed");
        if (options.mMineshaftMarkers && (!hasReferenceSeed || levelDat.getLong("RandomSeed") != options.mSeed)) {
            LOG_ERROR(LogAreaID::Level, "--mineshaft-markers requires level.dat with the same seed as --seed");
            return EXIT_INVALID;
        }
        if (hasReferenceSeed && levelDat.getLong("RandomSeed") != options.mSeed)
            LOG_WARN(LogAreaID::Level, "level.dat records seed %lld, comparing with --seed %lld",
                                       (long long) levelDat.getLong("RandomSeed"), (long long) options.mSeed);

        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        if (!options.mBehaviorPacks.empty()) {
            if (!hasReferenceSeed || levelDat.getLong("RandomSeed") != options.mSeed) {
                LOG_ERROR(LogAreaID::Level, "--behavior-packs requires a reference world with the same seed");
                return EXIT_INVALID;
            }
            BehaviorPackManager packs;
            packs.discover(options.mBehaviorPacks);
            if (packs.getPacks().empty()) {
                LOG_ERROR(LogAreaID::Level, "No behavior packs found in '%s'", options.mBehaviorPacks.c_str());
                return EXIT_INVALID;
            }
            ServerPaths::setRoot(copy.root());
            BiomeChunkGenDataRegistry::loadPacks(packs.getPacks());
            OverworldRulePlacer::loadPacks(packs.getPacks());
            JigsawStructures::packStructures().loadPacks(packs.getPacks());
        }
        const BiomeNames biomes;

        if (options.mHasColumn)
            return dumpColumn(storage, options, biomes);

        const BlockClassifier classifier;
        if (!options.mFar)
            return compareArea(storage, options, options.mArea, classifier, biomes);

        int result = 0;
        for (const int32_t distance: {1000, 10000}) {
            for (const int32_t signX: {1, -1}) {
                for (const int32_t signZ: {1, -1}) {
                    const int32_t chunkX = (distance * signX) >> 4;
                    const int32_t chunkZ = (distance * signZ) >> 4;
                    const ChunkArea area{chunkX - 3, chunkZ - 3, chunkX + 2, chunkZ + 2};
                    const int regionResult = compareArea(storage, options, area, classifier, biomes);
                    if (regionResult != 0)
                        result = regionResult;
                }
            }
        }

        return result;
    }
}

int main(int argc, char **argv) {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));

    int result = EXIT_INVALID;
    Options options;
    if (!parseOptions(argc, argv, options)) {
        printUsage();
    } else {
        try {
            gOrder = options.mOrder;
            gThreads = options.mThreads;
            result = run(options);
        } catch (const std::exception &exception) {
            LOG_ERROR(LogAreaID::Level, "%s", exception.what());
        }
    }

    // log endpoints write from a worker thread: drain the report before the process exits
    BedrockLog::flush();
    return result;
}
