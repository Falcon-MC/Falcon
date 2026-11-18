#include "World/Level/LevelGen/Overworld/Terrain/OverworldCarvers.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/BlockStateSet.h"
#include "World/Level/LevelGen/Overworld/OverworldBlockLists.h"
#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Overworld/Terrain/OverworldAquifer.h"
#include "World/Level/LevelGen/Random/GameRandom.h"

#include <cmath>

namespace {
    constexpr int32_t MIN_Y = OverworldGeneratorConstants::MIN_Y;
    // the game generates into a block volume 320 blocks tall, counted from MIN_Y
    constexpr int32_t VOLUME_HEIGHT = 320;
    constexpr int32_t RANGE = 8;
    constexpr float PI = 3.1415927f;
    constexpr float HALF_PI = 1.5707964f;

    constexpr int32_t CANYON_RARITY = 100;
    constexpr int32_t CANYON_MAX_Y = 312;
    constexpr int32_t CANYON_WIDTH_ROWS = 320;

    using GameRandom::nextFloat;
    using GameRandom::nextInt;
    using GameRandom::nextUnsigned;
    using GameRandom::toFloat;

    float lerp(const float *range, std::mt19937 &random) {
        if (range[0] == range[1])
            return range[0];

        return range[0] + (range[1] - range[0]) * nextFloat(random);
    }

    // the cave carver's replaceable blocks; glazed terracotta stands where terracotta is expected
    const BlockStateSet &caveReplaceable() {
        static const BlockStateSet names = {"minecraft:stone",
                                            "minecraft:dirt",
                                            "minecraft:coarse_dirt",
                                            "minecraft:podzol",
                                            "minecraft:grass_block",
                                            "minecraft:white_glazed_terracotta",
                                            "minecraft:orange_glazed_terracotta",
                                            "minecraft:magenta_glazed_terracotta",
                                            "minecraft:light_blue_glazed_terracotta",
                                            "minecraft:yellow_glazed_terracotta",
                                            "minecraft:lime_glazed_terracotta",
                                            "minecraft:pink_glazed_terracotta",
                                            "minecraft:gray_glazed_terracotta",
                                            "minecraft:silver_glazed_terracotta",
                                            "minecraft:cyan_glazed_terracotta",
                                            "minecraft:purple_glazed_terracotta",
                                            "minecraft:blue_glazed_terracotta",
                                            "minecraft:brown_glazed_terracotta",
                                            "minecraft:green_glazed_terracotta",
                                            "minecraft:red_glazed_terracotta",
                                            "minecraft:black_glazed_terracotta",
                                            "minecraft:sandstone",
                                            "minecraft:chiseled_sandstone",
                                            "minecraft:cut_sandstone",
                                            "minecraft:smooth_sandstone",
                                            "minecraft:red_sandstone",
                                            "minecraft:chiseled_red_sandstone",
                                            "minecraft:cut_red_sandstone",
                                            "minecraft:smooth_red_sandstone",
                                            "minecraft:mycelium",
                                            "minecraft:snow_layer",
                                            "minecraft:packed_ice",
                                            "minecraft:deepslate",
                                            "minecraft:calcite",
                                            "minecraft:sand",
                                            "minecraft:red_sand",
                                            "minecraft:suspicious_sand",
                                            "minecraft:gravel",
                                            "minecraft:dirt_with_roots",
                                            "minecraft:tuff",
                                            "minecraft:iron_ore",
                                            "minecraft:deepslate_iron_ore",
                                            "minecraft:raw_iron_block",
                                            "minecraft:copper_ore",
                                            "minecraft:deepslate_copper_ore",
                                            "minecraft:raw_copper_block"};
        return names;
    }

    // the canyon's carvable blocks
    const BlockStateSet &canyonReplaceable() {
        static const BlockStateSet names = {"minecraft:stone",
                                            "minecraft:granite",
                                            "minecraft:diorite",
                                            "minecraft:andesite",
                                            "minecraft:tuff",
                                            "minecraft:deepslate",
                                            "minecraft:calcite",
                                            "minecraft:dirt",
                                            "minecraft:coarse_dirt",
                                            "minecraft:mud",
                                            "minecraft:podzol",
                                            "minecraft:mycelium",
                                            "minecraft:grass_block",
                                            "minecraft:dirt_with_roots",
                                            "minecraft:hardened_clay",
                                            "minecraft:white_terracotta",
                                            "minecraft:orange_terracotta",
                                            "minecraft:magenta_terracotta",
                                            "minecraft:light_blue_terracotta",
                                            "minecraft:yellow_terracotta",
                                            "minecraft:lime_terracotta",
                                            "minecraft:pink_terracotta",
                                            "minecraft:gray_terracotta",
                                            "minecraft:light_gray_terracotta",
                                            "minecraft:cyan_terracotta",
                                            "minecraft:purple_terracotta",
                                            "minecraft:blue_terracotta",
                                            "minecraft:brown_terracotta",
                                            "minecraft:green_terracotta",
                                            "minecraft:red_terracotta",
                                            "minecraft:black_terracotta",
                                            "minecraft:sandstone",
                                            "minecraft:chiseled_sandstone",
                                            "minecraft:cut_sandstone",
                                            "minecraft:smooth_sandstone",
                                            "minecraft:red_sandstone",
                                            "minecraft:chiseled_red_sandstone",
                                            "minecraft:cut_red_sandstone",
                                            "minecraft:smooth_red_sandstone",
                                            "minecraft:sand",
                                            "minecraft:red_sand",
                                            "minecraft:suspicious_sand",
                                            "minecraft:iron_ore",
                                            "minecraft:deepslate_iron_ore",
                                            "minecraft:raw_iron_block",
                                            "minecraft:copper_ore",
                                            "minecraft:deepslate_copper_ore",
                                            "minecraft:raw_copper_block",
                                            "minecraft:snow",
                                            "minecraft:snow_layer",
                                            "minecraft:packed_ice",
                                            "minecraft:gravel",
                                            "minecraft:sulfur",
                                            "minecraft:cinnabar"};
        return names;
    }

    const BlockStateSet &dirt() {
        static const BlockStateSet names = {"minecraft:dirt", "minecraft:coarse_dirt"};
        return names;
    }

    const BlockState &airState() {
        static const BlockState state;
        return state;
    }

    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    const BlockState &grassState() {
        static const BlockState state = VanillaBlocks::GRASS_BLOCK().toBlockState();
        return state;
    }

    const BlockState &myceliumState() {
        static const BlockState state = VanillaBlocks::MYCELIUM().toBlockState();
        return state;
    }

    const BlockState &sandstoneState() {
        static const BlockState state = VanillaBlocks::SANDSTONE().toBlockState();
        return state;
    }

    bool isGrassBlock(const BlockState &state) {
        static const BlockStateSet GRASS = {VanillaBlocks::GRASS_BLOCK().getIdentifier()};
        return GRASS.contains(state);
    }

    // what a carved grass or mycelium leaves on the dirt below it, null for any other block
    const BlockState *spreadingTop(const BlockState &state) {
        static const BlockStateSet MYCELIUM = {VanillaBlocks::MYCELIUM().getIdentifier()};
        if (isGrassBlock(state))
            return &grassState();

        return MYCELIUM.contains(state) ? &myceliumState() : nullptr;
    }

    // fluid below the aquifer's level, air above it, nothing where the aquifer keeps a barrier
    void fillCarved(LevelChunk &chunk, OverworldAquifer &aquifer, int32_t x, int32_t y, int32_t z, int32_t worldX,
                    int32_t worldZ) {
        const OverworldAquifer::Substance substance = aquifer.compute(worldX, y, worldZ);
        if (substance.mDensity > 0.0f)
            return;

        if (y < substance.mFluidLevel)
            chunk.setBlock(x, y, z, substance.mLava ? lavaState() : waterState());
        else
            chunk.setBlock(x, y, z, airState());
    }

    // CanyonFeature's carve; rows count from the bottom of the world
    void carveCanyonBox(LevelChunk &chunk, OverworldAquifer &aquifer, const float *widths, float centerX, float centerY,
                        float centerZ, float horizontalRadius, float verticalRadius, int32_t minX, int32_t maxX,
                        int32_t minRow, int32_t maxRow, int32_t minZ, int32_t maxZ) {
        if (minX >= maxX || minZ >= maxZ || maxRow <= minRow)
            return;

        const int32_t baseX = chunk.getX() << 4;
        const int32_t baseZ = chunk.getZ() << 4;
        for (int32_t x = minX; x < maxX; x++) {
            const float dx = ((static_cast<float>(baseX + x) + 0.5f) - centerX) / horizontalRadius;
            const float dx2 = dx * dx;
            for (int32_t z = minZ; z < maxZ; z++) {
                const float dz = ((static_cast<float>(baseZ + z) + 0.5f) - centerZ) / horizontalRadius;
                const float horizontal = dz * dz + dx2;
                if (!(horizontal < 1.0f))
                    continue;

                bool grassAbove = false;
                for (int32_t row = maxRow; row > minRow; row--) {
                    const float dy = ((static_cast<float>(row - 1) + 0.5f) - centerY) / verticalRadius;
                    if (!((dy * dy) / 6.0f + widths[row - 2] * horizontal < 1.0f))
                        continue;

                    const int32_t y = row + MIN_Y;
                    const BlockState &block = chunk.getBlock(x, y, z);
                    grassAbove |= isGrassBlock(block);
                    if (!canyonReplaceable().contains(block))
                        continue;

                    fillCarved(chunk, aquifer, x, y, z, baseX + x, baseZ + z);
                    if (grassAbove && dirt().contains(chunk.getBlock(x, y - 1, z)))
                        chunk.setBlock(x, y - 1, z, grassState());
                }
            }
        }
    }
}

OverworldCarvers::OverworldCarvers(int64_t seed) : mSeed(static_cast<uint32_t>(seed)) {}

void OverworldCarvers::carve(LevelChunk &chunk, OverworldAquifer &aquifer) const {
    // underground_extra_cave_carver_feature then underground_cave_carver_feature: every overworld biome places
    // both, in this order, in its pregeneration_pass
    static const CaveSettings FEATURES[2] = {{66, 103, {0.1f, 0.9f}, {0.7f, 1.4f}, {0.8f, 1.3f}, {-1.0f, -0.4f}},
                                             {33, 236, {0.1f, 0.9f}, {0.7f, 1.4f}, {0.8f, 1.3f}, {-1.0f, -0.4f}}};
    _carveCaves(FEATURES[0], chunk, aquifer);
    _carveCaves(FEATURES[1], chunk, aquifer);
    _carveCanyons(chunk, aquifer);
}

void OverworldCarvers::_carveCaves(const CaveSettings &settings, LevelChunk &chunk, OverworldAquifer &aquifer) const {
    // the feature rule seeds the chunk random with the level seed before placing
    std::mt19937 random(mSeed);
    const uint32_t multiplierX = static_cast<uint32_t>(nextInt(random)) | 1u;
    const uint32_t multiplierZ = static_cast<uint32_t>(nextInt(random)) | 1u;

    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const float centerX = static_cast<float>((chunkX << 4) | 8);
    const float centerZ = static_cast<float>((chunkZ << 4) | 8);

    // the game caches each source chunk's segments and carves every one that can reach the chunk, where the
    // uncached path would stop a tunnel at its first unreachable step
    std::vector<CaveSegment> segments;
    for (int32_t sourceX = chunkX - RANGE; sourceX <= chunkX + RANGE; sourceX++) {
        for (int32_t sourceZ = chunkZ - RANGE; sourceZ <= chunkZ + RANGE; sourceZ++) {
            random.seed(
                    mSeed
                    ^ (multiplierX * static_cast<uint32_t>(sourceX) + multiplierZ * static_cast<uint32_t>(sourceZ)));
            segments.clear();
            _addCaves(settings, random, sourceX, sourceZ, segments);

            for (const CaveSegment &segment: segments) {
                const float dx = segment.mX - centerX;
                const float dz = segment.mZ - centerZ;
                const int32_t remaining = segment.mLength - segment.mStep;
                const float reach = (segment.mThickness + 2.0f) + 16.0f;
                if (!(reach * reach >= (dz * dz + dx * dx) - static_cast<float>(remaining * remaining)))
                    continue;

                const float margin = (segment.mHorizontalRadius + segment.mHorizontalRadius) + 16.0f;
                const float blockX = static_cast<float>(static_cast<int32_t>(std::floor(segment.mX)));
                if (!(centerX - margin <= blockX && blockX <= margin + centerX))
                    continue;

                const float blockZ = static_cast<float>(static_cast<int32_t>(std::floor(segment.mZ)));
                if (!(centerZ - margin <= blockZ && blockZ <= margin + centerZ))
                    continue;

                _carveSegment(segment, settings.mHeightLimit, chunk, aquifer);
            }
        }
    }
}

void OverworldCarvers::_addCaves(const CaveSettings &settings, std::mt19937 &random, int32_t sourceX, int32_t sourceZ,
                                 std::vector<CaveSegment> &segments) {
    const int32_t count = nextInt(random, nextInt(random, nextInt(random, 40) + 1) + 1);
    const int32_t skip = nextInt(random, settings.mSkipCarveChance);
    if (count <= 0 || skip != 0)
        return;

    for (int32_t cave = 0; cave < count; cave++) {
        const int32_t z = (sourceZ << 4) + nextInt(random, 16);
        const int32_t y = nextInt(random, settings.mHeightLimit) + 8;
        const int32_t x = (sourceX << 4) + nextInt(random, 16);

        float shape[3];
        shape[0] = lerp(settings.mHorizontalRadiusMultiplier, random);
        shape[1] = lerp(settings.mVerticalRadiusMultiplier, random);
        shape[2] = lerp(settings.mFloorLevel, random);

        const float startX = static_cast<float>(x);
        const float startY = static_cast<float>(y);
        const float startZ = static_cast<float>(z);

        int32_t tunnels = 1;
        if (nextInt(random, 4) == 0) {
            _addRoom(settings, random, startX, startY, startZ, shape, segments);
            tunnels = nextInt(random, 4) + 1;
        }

        for (int32_t tunnel = 0; tunnel < tunnels; tunnel++) {
            const float yawRoll = nextFloat(random);
            const float yaw = yawRoll * PI + yawRoll * PI;
            const float pitchRoll = nextFloat(random) + -0.5f;
            const float pitch = (pitchRoll + pitchRoll) * 0.125f;

            const float thicknessRoll = nextFloat(random);
            float thickness = nextFloat(random) + (thicknessRoll + thicknessRoll);
            if (nextInt(random, 10) == 0) {
                const float first = nextFloat(random);
                thickness = thickness * ((nextFloat(random) * first) * 3.0f + 1.0f);
            }

            const int32_t length = 112 - nextInt(random, 28);
            _addTunnel(random, startX, startY, startZ, thickness, yaw, pitch, 0, length, shape, segments);
        }
    }
}

void OverworldCarvers::_addRoom(const CaveSettings &settings, std::mt19937 &random, float x, float y, float z,
                                const float *shape, std::vector<CaveSegment> &segments) {
    const float size = nextFloat(random) * 6.0f;
    std::mt19937 local(static_cast<uint32_t>(nextInt(random)));

    float yScale = settings.mYScale[0];
    if (settings.mYScale[0] != settings.mYScale[1])
        yScale = yScale + (settings.mYScale[1] - yScale) * nextFloat(local);

    const float thickness = size + 1.0f;
    const uint32_t length = 112u - nextUnsigned(local) % 28u;
    const float wave = std::sin((static_cast<float>(length >> 1) * PI) / static_cast<float>(length));
    const float radius = wave * (0.0f + thickness) + 1.5f;

    CaveSegment segment{};
    segment.mX = x;
    segment.mY = y;
    segment.mZ = z;
    segment.mHorizontalRadius = radius * shape[0];
    segment.mVerticalRadius = (yScale * radius) * shape[1];
    segment.mFloorLevel = shape[2];
    segment.mStep = static_cast<int32_t>(length >> 1);
    segment.mLength = static_cast<int32_t>(length);
    segment.mThickness = thickness;
    segments.push_back(segment);
}

void OverworldCarvers::_addTunnel(std::mt19937 &parent, float x, float y, float z, float thickness, float yaw,
                                  float pitch, int32_t step, int32_t length, const float *shape,
                                  std::vector<CaveSegment> &segments) {
    // the radius multipliers only shape rooms
    std::mt19937 random(static_cast<uint32_t>(nextInt(parent)));
    if (length <= 0)
        length = static_cast<int32_t>(112u - nextUnsigned(random) % 28u);

    const uint32_t half = static_cast<uint32_t>(length) >> 1;
    uint32_t branch = half != 0 ? nextUnsigned(random) % half : 0u;
    const float steepness = nextUnsigned(random) % 6u != 0u ? 0.7f : 0.92f;
    if (length - step <= 0)
        return;

    branch += static_cast<uint32_t>(length) >> 2;
    const float lengthF = static_cast<float>(length);

    float yawChange = 0.0f;
    float pitchChange = 0.0f;
    for (;; step++) {
        const float wave = std::sin((static_cast<float>(step) * PI) / lengthF);
        const float width = 0.0f + thickness;

        const float pitchCos = std::cos(pitch);
        const float pitchSin = std::sin(pitch);
        x = std::cos(yaw) * pitchCos + x;
        y = pitchSin + y;
        z = std::sin(yaw) * pitchCos + z;

        const uint32_t rolls[6] = {random(), random(), random(), random(), random(), random()};
        const float pitchDrift = (toFloat(rolls[0]) - toFloat(rolls[1])) * toFloat(rolls[2]);
        const float yawDrift = (toFloat(rolls[3]) - toFloat(rolls[4])) * toFloat(rolls[5]);
        const float nextPitch = pitch * steepness + pitchChange * 0.1f;
        const float nextYaw = yaw + yawChange * 0.1f;
        yawChange = yawChange * 0.75f + yawDrift * 4.0f;
        pitchChange = pitchChange * 0.9f + pitchDrift * 2.0f;
        pitch = nextPitch;
        yaw = nextYaw;

        const float radius = wave * width + 1.5f;
        if (thickness > 1.0f && branch == static_cast<uint32_t>(step)) {
            const float childPitch = pitch / 3.0f;
            const float firstThickness = toFloat(random()) * 0.5f + 0.5f;
            _addTunnel(random, x, y, z, firstThickness, -HALF_PI + yaw, childPitch, static_cast<int32_t>(branch),
                       length, shape, segments);
            const float secondThickness = toFloat(random()) * 0.5f + 0.5f;
            _addTunnel(random, x, y, z, secondThickness, yaw + HALF_PI, childPitch, static_cast<int32_t>(branch),
                       length, shape, segments);
            return;
        }

        if (random() % 4u != 0u) {
            CaveSegment segment{};
            segment.mX = x;
            segment.mY = y;
            segment.mZ = z;
            segment.mHorizontalRadius = radius;
            segment.mVerticalRadius = radius;
            segment.mFloorLevel = shape[2];
            segment.mStep = step;
            segment.mLength = length;
            segment.mThickness = thickness;
            segments.push_back(segment);
        }

        if (step + 1 >= length)
            return;
    }
}

void OverworldCarvers::_carveSegment(const CaveSegment &segment, int32_t heightLimit, LevelChunk &chunk,
                                     OverworldAquifer &aquifer) {
    const int32_t baseX = chunk.getX() << 4;
    const int32_t baseZ = chunk.getZ() << 4;
    const float radius = segment.mHorizontalRadius;
    const float verticalRadius = segment.mVerticalRadius;

    int32_t top = VOLUME_HEIGHT - 2;
    if (top >= heightLimit)
        top = heightLimit;

    int32_t minX = static_cast<int32_t>(std::floor(segment.mX - radius)) - baseX - 1;
    if (minX <= 0)
        minX = 0;

    int32_t maxX = static_cast<int32_t>(std::floor(segment.mX + radius)) - baseX;
    if (maxX >= 15)
        maxX = 15;
    maxX++;

    const int32_t highest = static_cast<int32_t>(std::floor(verticalRadius + segment.mY));
    int32_t maxRow = (highest > 0 ? highest : 0) + 1;
    if (top <= highest)
        maxRow = top;

    const int32_t lowest = static_cast<int32_t>(std::floor(segment.mY - verticalRadius)) - 1;
    int32_t minRow = lowest >= 2 ? lowest : 1;
    if (lowest > maxRow)
        minRow = maxRow;

    int32_t minZ = static_cast<int32_t>(std::floor(segment.mZ - radius)) - baseZ - 1;
    if (minZ <= 0)
        minZ = 0;

    int32_t maxZ = static_cast<int32_t>(std::floor(segment.mZ + radius)) - baseZ;
    if (maxZ >= 15)
        maxZ = 15;
    maxZ++;

    for (int32_t x = minX; x < maxX; x++) {
        const float dx = ((static_cast<float>(x + baseX) + 0.5f) - segment.mX) / radius;
        const float dx2 = dx * dx;
        for (int32_t z = minZ; z < maxZ; z++) {
            const float dz = ((static_cast<float>(z + baseZ) + 0.5f) - segment.mZ) / radius;
            const float dz2 = dz * dz;
            if (!(dx2 + dz2 < 1.0f))
                continue;

            for (int32_t row = maxRow; row > minRow; row--) {
                const float dy = ((static_cast<float>(row - 1) + 0.5f) - segment.mY) / verticalRadius;
                if (segment.mFloorLevel >= dy || !((dy * dy + dx2) + dz2 < 1.0f))
                    continue;

                const int32_t y = row + MIN_Y;
                const BlockState &block = chunk.getBlock(x, y, z);
                if (!caveReplaceable().contains(block))
                    continue;

                // the carved grass or mycelium spreads onto the dirt below
                const BlockState *spread = spreadingTop(block);

                const BlockStateSet &sand = OverworldBlockLists::sand();
                if (row <= VOLUME_HEIGHT - 3 && sand.contains(chunk.getBlock(x, y + 1, z))
                    && sand.contains(chunk.getBlock(x, y + 2, z)) && sand.contains(chunk.getBlock(x, y + 3, z)))
                    chunk.setBlock(x, y + 1, z, sandstoneState());

                fillCarved(chunk, aquifer, x, y, z, baseX + x, baseZ + z);
                if (spread != nullptr && dirt().contains(chunk.getBlock(x, y - 1, z)))
                    chunk.setBlock(x, y - 1, z, *spread);
            }
        }
    }
}

void OverworldCarvers::_carveCanyons(LevelChunk &chunk, OverworldAquifer &aquifer) const {
    std::mt19937 random(mSeed);
    const uint32_t multiplierX = static_cast<uint32_t>(nextInt(random)) | 1u;
    const uint32_t multiplierZ = static_cast<uint32_t>(nextInt(random)) | 1u;

    for (int32_t sourceX = chunk.getX() - RANGE; sourceX <= chunk.getX() + RANGE; sourceX++) {
        for (int32_t sourceZ = chunk.getZ() - RANGE; sourceZ <= chunk.getZ() + RANGE; sourceZ++) {
            random.seed(
                    mSeed
                    ^ (multiplierX * static_cast<uint32_t>(sourceX) + multiplierZ * static_cast<uint32_t>(sourceZ)));
            _addCanyon(random, sourceX, sourceZ, chunk, aquifer);
        }
    }
}

void OverworldCarvers::_addCanyon(std::mt19937 &random, int32_t sourceX, int32_t sourceZ, LevelChunk &chunk,
                                  OverworldAquifer &aquifer) {
    // heights are rows from the bottom of the world
    if (nextInt(random, CANYON_RARITY) != 0)
        return;

    const int32_t startX = (sourceX << 4) + nextInt(random, 16);
    const int32_t startRow = nextInt(random, 58) + 64 + 10;
    random.discard(2);
    const int32_t startZ = (sourceZ << 4) + nextInt(random, 16);

    const float yawRoll = nextFloat(random) * PI;
    const float pitchRoll = nextFloat(random) + -0.5f;
    const float thicknessRoll = nextFloat(random) * 3.0f;
    const float thickness = nextFloat(random) * 3.0f + thicknessRoll;
    // the roll for a wide canyon is drawn, but these parameters never allow one
    nextFloat(random);
    const float yScale = 4.0f;

    float pitch = pitchRoll * 0.25f;
    float yaw = yawRoll * 2.0f;
    float x = static_cast<float>(startX);
    float y = static_cast<float>(startRow);
    float z = static_cast<float>(startZ);

    const uint32_t seed = static_cast<uint32_t>(nextInt(random));
    std::mt19937 walk(seed);
    std::mt19937 upper(seed);
    const uint32_t length = 120u - nextUnsigned(walk) % 30u;

    float widths[CANYON_WIDTH_ROWS];
    float roll = nextFloat(walk);
    float width = nextFloat(walk) * roll + 1.0f;
    widths[0] = width * width;
    for (int32_t row = 1; row < CANYON_WIDTH_ROWS; row++) {
        std::mt19937 &rows = row < 128 ? walk : upper;
        if (nextInt(rows, 3) == 0) {
            roll = nextFloat(rows);
            width = nextFloat(rows) * roll + 1.0f;
        }
        widths[row] = width * width;
    }

    const int32_t baseX = chunk.getX() << 4;
    const int32_t baseZ = chunk.getZ() << 4;
    const float centerX = static_cast<float>(baseX | 8);
    const float centerZ = static_cast<float>(baseZ | 8);
    const float reach = (2.0f + thickness) + 16.0f;
    const float reachSquared = reach * reach;
    const float lengthF = static_cast<float>(length);

    float yawChange = 0.0f;
    float pitchChange = 0.0f;
    int32_t remaining = static_cast<int32_t>(length);
    for (uint32_t step = 1;; step++) {
        const float wave = std::sin((static_cast<float>(static_cast<int32_t>(step - 1)) * PI) / lengthF);
        const float radius = wave * thickness + 1.5f;
        const uint32_t horizontalRoll = walk();
        const uint32_t verticalRoll = walk();

        const float pitchCos = std::cos(pitch);
        const float pitchSin = std::sin(pitch);
        x = std::cos(yaw) * pitchCos + x;
        y = pitchSin + y;
        z = std::sin(yaw) * pitchCos + z;

        const uint32_t rolls[6] = {walk(), walk(), walk(), walk(), walk(), walk()};
        const float pitchDrift = (toFloat(rolls[0]) - toFloat(rolls[1])) * toFloat(rolls[2]);
        const float yawDrift = (toFloat(rolls[3]) - toFloat(rolls[4])) * toFloat(rolls[5]);
        const float nextYawChange = yawDrift * 4.0f + yawChange * 0.5f;
        const float nextPitchChange = pitchDrift * 2.0f + pitchChange * 0.8f;
        const float nextPitch = pitchChange * 0.05f + pitch * 0.7f;
        const float nextYaw = yaw * 1.0f + yawChange * 0.05f;

        if ((walk() & 3u) != 0u) {
            const float dx = x - centerX;
            const float dz = z - centerZ;
            const float remainingF = static_cast<float>(remaining);
            if ((dz * dz + dx * dx) - remainingF * remainingF > reachSquared)
                return;

            const float horizontalRadius = (toFloat(horizontalRoll) * 0.25f + 0.75f) * radius;
            const float doubled = horizontalRadius + horizontalRadius;
            if (!(centerX + 16.0f + doubled < x || centerZ + 16.0f + doubled < z || x < centerX - 16.0f - doubled
                  || z < centerZ - 16.0f - doubled)) {
                const float verticalRadius = (toFloat(verticalRoll) * 0.25f + 0.75f) * (radius * yScale);

                int32_t minX = static_cast<int32_t>((std::floor(x - horizontalRadius) - static_cast<float>(baseX))
                                                    + -1.0f);
                int32_t maxX = static_cast<int32_t>((std::floor(x + horizontalRadius) - static_cast<float>(baseX))
                                                    + 1.0f);
                const int32_t lowest = static_cast<int32_t>(std::floor(y - verticalRadius) + -1.0f);
                const int32_t highest = static_cast<int32_t>(std::floor(y + verticalRadius) + 1.0f);
                int32_t minZ = static_cast<int32_t>((std::floor(z - horizontalRadius) - static_cast<float>(baseZ))
                                                    + -1.0f);
                int32_t maxZ = static_cast<int32_t>((std::floor(z + horizontalRadius) - static_cast<float>(baseZ))
                                                    + 1.0f);
                if (minX <= 0)
                    minX = 0;
                if (maxX >= 16)
                    maxX = 16;
                if (minZ <= 0)
                    minZ = 0;
                if (maxZ >= 16)
                    maxZ = 16;

                int32_t maxRow = highest >= 2 ? highest : 1;
                if (CANYON_MAX_Y < highest)
                    maxRow = CANYON_MAX_Y;
                int32_t minRow = lowest >= 2 ? lowest : 1;
                if (maxRow < lowest)
                    minRow = maxRow;

                carveCanyonBox(chunk, aquifer, widths, x, y, z, horizontalRadius, verticalRadius, minX, maxX, minRow,
                               maxRow, minZ, maxZ);
            }
        }

        if (step >= length)
            return;

        remaining--;
        yawChange = nextYawChange;
        pitchChange = nextPitchChange;
        pitch = nextPitch;
        yaw = nextYaw;
    }
}
