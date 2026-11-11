#include "World/Level/LevelGen/Nether/Feature/NetherFeatures.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Random/GameRandom.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
    using GameRandom::nextFloat;
    using GameRandom::nextInt;

    // the dimension's build limits
    constexpr int32_t MIN_HEIGHT = 0;
    constexpr int32_t MAX_HEIGHT = 128;

    bool isAir(const BlockState &state) {
        return state.mName == "minecraft:air";
    }

    bool isAirAt(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        return isAir(blocks.getBlockAt(x, y, z));
    }

    bool isSolidAt(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        return BlockSupport::isSolid(blocks.getBlockAt(x, y, z));
    }

    bool isLava(const BlockState &state) {
        return state.mName == "minecraft:lava" || state.mName == "minecraft:flowing_lava";
    }

    bool isNetherrackOrWart(const BlockState &state) {
        return state.mName == "minecraft:netherrack" || state.mName == "minecraft:nether_wart_block";
    }

    const BlockState &basalt() {
        static const BlockState state = VanillaBlocks::BASALT().toBlockState();
        return state;
    }

    const BlockState &lava() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    const BlockState &magma() {
        static const BlockState state = VanillaBlocks::MAGMA().toBlockState();
        return state;
    }

    const BlockState &netherWart() {
        static const BlockState state = VanillaBlocks::NETHER_WART_BLOCK().toBlockState();
        return state;
    }

    const BlockState &shroomlight() {
        static const BlockState state = VanillaBlocks::SHROOMLIGHT().toBlockState();
        return state;
    }

    std::vector<BlockState> agesOf(const BlockState &base, const char *stateName) {
        std::vector<BlockState> ages;
        for (int32_t age = 0; age < 26; age++) {
            Tag states = base.mStates;
            states.putInt(stateName, age);
            ages.emplace_back(base.mName, states);
        }
        return ages;
    }

    const BlockState &weepingVines(int32_t age) {
        static const std::vector<BlockState> ages = agesOf(VanillaBlocks::WEEPING_VINES().toBlockState(),
                                                           "weeping_vines_age");
        return ages[static_cast<size_t>(age)];
    }

    const BlockState &twistingVines(int32_t age) {
        static const std::vector<BlockState> ages = agesOf(VanillaBlocks::TWISTING_VINES().toBlockState(),
                                                           "twisting_vines_age");
        return ages[static_cast<size_t>(age)];
    }

    // a vine's drawn base, doubled one time in six, reset to 1 one time in five
    int32_t vineLength(int32_t base, std::mt19937 &random) {
        int32_t length = base << (nextInt(random, 6) == 0 ? 1 : 0);
        if (nextInt(random, 5) == 0)
            length = 1;
        return length;
    }

    // sub_1432ECE10
    void growWeepingVines(BlockManager &blocks, int32_t x, int32_t y, int32_t z, int32_t length, int32_t minAge,
                          int32_t maxAge, std::mt19937 &random) {
        for (int32_t i = 0; i <= length; i++, y--) {
            if (!isAirAt(blocks, x, y, z))
                continue;

            blocks.setBlockStateAt(x, y, z, weepingVines(minAge + nextInt(random, maxAge - minAge)));
            if (i == length || !isAirAt(blocks, x, y - 1, z))
                return;
        }
    }

    void hangVines(BlockManager &blocks, bool vines, int32_t bound, int32_t x, int32_t y, int32_t z,
                   std::mt19937 &random) {
        if (!vines || nextInt(random, bound) != 0 || !isAirAt(blocks, x, y - 1, z))
            return;

        const int32_t base = nextInt(random, 5);
        growWeepingVines(blocks, x, y - 1, z, vineLength(base, random), 23, 25, random);
    }

    bool isFungusBase(const BlockState &state) {
        return state.mName == "minecraft:soul_soil" || state.mName == "minecraft:warped_nylium"
               || state.mName == "minecraft:crimson_nylium" || IFeature::isSupportDirt(state);
    }

    bool isStemReplaceable(const BlockState &state) {
        return BlockSupport::isReplaceable(state) || state.mName == "minecraft:crimson_fungus"
               || state.mName == "minecraft:warped_fungus";
    }

    void placeHat(BlockManager &blocks, int32_t x, int32_t y, int32_t z, int32_t row, int32_t height, int32_t hatBottom,
                  int32_t radius, int32_t dx, int32_t dz, const BlockState &hat, bool vines, std::mt19937 &random) {
        const BlockState &here = blocks.getBlockAt(x, y, z);
        if (BlockSupport::isSolid(here) || !BlockSupport::isReplaceable(here))
            return;

        const bool edgeX = std::abs(dx) == radius;
        const bool edgeZ = std::abs(dz) == radius;
        const bool inside = !edgeX && !edgeZ && row != height;
        if (row < hatBottom + 3) {
            if (inside)
                return;

            if (blocks.getBlockAt(x, y - 1, z).mName == hat.mName) {
                blocks.setBlockStateAt(x, y, z, hat);
            } else if (nextFloat(random) < 0.15f) {
                blocks.setBlockStateAt(x, y, z, hat);
                hangVines(blocks, vines, 11, x, y, z, random);
            }
            return;
        }

        const float roll = nextFloat(random);
        if (inside) {
            if (roll < 0.1f) {
                blocks.setBlockStateAt(x, y, z, shroomlight());
            } else if (nextFloat(random) < 0.2f) {
                blocks.setBlockStateAt(x, y, z, hat);
                hangVines(blocks, vines, 10, x, y, z, random);
            }
            return;
        }

        const bool corner = edgeX && edgeZ;
        if (roll < (corner ? 0.7f : 0.98f)) {
            blocks.setBlockStateAt(x, y, z, hat);
            hangVines(blocks, vines, corner ? 12 : 14, x, y, z, random);
        } else if (nextFloat(random) < 0.02f) {
            blocks.setBlockStateAt(x, y, z, shroomlight());
        }
    }

    bool placeHugeFungus(BlockManager &blocks, int32_t x, int32_t originY, int32_t z, std::mt19937 &random,
                         const BlockState &stem, const BlockState &hat, bool vines) {
        const int32_t base = nextInt(random, 10);
        const int32_t height = (base + 4) << (nextInt(random, 12) == 0 ? 1 : 0);
        int32_t y = originY;
        if (y <= MIN_HEIGHT || height + y + 1 >= MAX_HEIGHT)
            return false;

        for (int32_t tries = 0;; tries++) {
            const BlockState &below = blocks.getBlockAt(x, y - 1, z);
            if (isLava(blocks.getBlockAt(x, y, z)) || isLava(below))
                return false;
            if (isFungusBase(below))
                break;
            if (tries == 100)
                return false;

            y--;
            if (y <= MIN_HEIGHT)
                return false;
        }

        const int32_t thick = nextFloat(random) < 0.06f ? 1 : 0;
        const int32_t stemHeight = std::max(height, 1);
        for (int32_t dx = -thick; dx <= thick; dx++) {
            for (int32_t dz = -thick; dz <= thick; dz++) {
                const bool corner = thick != 0 && std::abs(dx) == thick && std::abs(dz) == thick;
                for (int32_t dy = 0; dy < stemHeight; dy++) {
                    if (isStemReplaceable(blocks.getBlockAt(x + dx, y + dy, z + dz))
                        && (!corner || nextFloat(random) < 0.1f))
                        blocks.setBlockStateAt(x + dx, y + dy, z + dz, stem);
                }
            }
        }

        const int32_t hatHeight = std::min(nextInt(random, height / 3 + 1) + 5, height);
        const int32_t hatBottom = height - hatHeight;
        for (int32_t row = hatBottom; row <= height; row++) {
            const int32_t narrow = (row < height - nextInt(random, 3) ? 1 : 0) + 1;
            const int32_t radius = (hatHeight >= 9 && row < hatBottom + 4 ? 3 : narrow) + thick;
            for (int32_t dx = -radius; dx <= radius; dx++) {
                for (int32_t dz = -radius; dz <= radius; dz++)
                    placeHat(blocks, x + dx, y + row, z + dz, row, height, hatBottom, radius, dx, dz, hat, vines,
                             random);
            }
        }
        return true;
    }

    int32_t countNetherrackOrWart(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        static const int32_t SIDES[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};
        int32_t count = 0;
        for (const auto &side: SIDES)
            count += isNetherrackOrWart(blocks.getBlockAt(x + side[0], y + side[1], z + side[2])) ? 1 : 0;
        return count;
    }

    // sub_14D206C90
    bool deltaFits(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        return !isLava(blocks.getBlockAt(x, y, z)) && isAirAt(blocks, x, y + 1, z) && !isAirAt(blocks, x, y - 1, z)
               && !isAirAt(blocks, x, y, z - 1) && !isAirAt(blocks, x, y, z + 1) && !isAirAt(blocks, x + 1, y, z)
               && !isAirAt(blocks, x - 1, y, z);
    }

    void placeBasaltIfBacked(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        if (isAirAt(blocks, x, y, z) && !isAirAt(blocks, x, y + 1, z))
            blocks.setBlockStateAt(x, y, z, basalt());
    }

    // the first floor of a column, walking at most `steps` blocks; false when the floor is not basalt or blackstone
    bool placeColumn(BlockManager &blocks, int32_t x, int32_t startY, int32_t z, int32_t steps, int32_t height) {
        int32_t y = startY;
        const int32_t walk = isSolidAt(blocks, x, y, z) ? 1 : -1;
        for (int32_t remaining = steps; y > MIN_HEIGHT && y < MAX_HEIGHT && remaining > 0; remaining--, y += walk) {
            if (isSolidAt(blocks, x, y, z) || !isSolidAt(blocks, x, y - 1, z))
                continue;

            const std::string &below = blocks.getBlockAt(x, y - 1, z).mName;
            if (below != "minecraft:basalt" && below != "minecraft:blackstone")
                return false;

            int32_t left = height;
            while (y < MAX_HEIGHT && left >= 0 && blocks.getBlockAt(x, y, z).mName != "minecraft:bedrock") {
                blocks.setBlockStateAt(x, y, z, basalt());
                y++;
                left--;
            }
            return true;
        }
        return false;
    }
}

bool NetherFeatures::placeHugeCrimsonFungus(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                            std::mt19937 &random) {
    static const BlockState stem = VanillaBlocks::CRIMSON_STEM().toBlockState();
    return placeHugeFungus(blocks, x, y, z, random, stem, netherWart(), true);
}

bool NetherFeatures::placeHugeWarpedFungus(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                           std::mt19937 &random) {
    static const BlockState stem = VanillaBlocks::WARPED_STEM().toBlockState();
    static const BlockState hat = VanillaBlocks::WARPED_WART_BLOCK().toBlockState();
    return placeHugeFungus(blocks, x, y, z, random, stem, hat, false);
}

bool NetherFeatures::placeWeepingVines(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    if (!isAirAt(blocks, x, y, z) || !isNetherrackOrWart(blocks.getBlockAt(x, y + 1, z)))
        return false;

    blocks.setBlockStateAt(x, y, z, netherWart());
    for (int32_t i = 0; i < 200; i++) {
        const int32_t blockX = x + nextInt(random, 12) - 6;
        const int32_t blockY = y + nextInt(random, 7) - 5;
        const int32_t blockZ = z + nextInt(random, 12) - 6;
        if (isAirAt(blocks, blockX, blockY, blockZ) && countNetherrackOrWart(blocks, blockX, blockY, blockZ) == 1)
            blocks.setBlockStateAt(blockX, blockY, blockZ, netherWart());
    }

    for (int32_t i = 0; i < 100; i++) {
        const int32_t blockX = x + nextInt(random, 16) - 8;
        const int32_t blockY = y + nextInt(random, 9) - 8;
        const int32_t blockZ = z + nextInt(random, 16) - 8;
        if (!isAirAt(blocks, blockX, blockY, blockZ)
            || !isNetherrackOrWart(blocks.getBlockAt(blockX, blockY + 1, blockZ)))
            continue;

        const int32_t base = nextInt(random, 7) + 1;
        growWeepingVines(blocks, blockX, blockY, blockZ, vineLength(base, random), 17, 25, random);
    }
    return true;
}

bool NetherFeatures::placeTwistingVines(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    // raw draws: the difference of two values below 10
    const auto spread = [&random]() {
        const int32_t first = static_cast<int32_t>(random() % 10u);
        return first - static_cast<int32_t>(random() % 10u);
    };

    for (int32_t i = 0; i < 100; i++) {
        const int32_t offsetX = spread();
        const int32_t offsetZ = spread();
        const int32_t blockX = x + offsetX - 5;
        int32_t blockY = y + nextInt(random, 5) - 4;
        const int32_t blockZ = z + offsetZ - 5;
        if (!isAirAt(blocks, blockX, blockY, blockZ))
            continue;

        const std::string &below = blocks.getBlockAt(blockX, blockY - 1, blockZ).mName;
        if (below != "minecraft:netherrack" && below != "minecraft:warped_nylium"
            && below != "minecraft:warped_wart_block")
            continue;

        const int32_t base = nextInt(random, 7) + 1;
        const int32_t length = vineLength(base, random);
        for (int32_t segment = 0; segment <= length; segment++, blockY++) {
            if (!isAirAt(blocks, blockX, blockY, blockZ))
                continue;

            blocks.setBlockStateAt(blockX, blockY, blockZ, twistingVines(nextInt(random, 25)));
            if (segment == length || !isAirAt(blocks, blockX, blockY + 1, blockZ))
                break;
        }
    }
    return true;
}

bool NetherFeatures::placeBasaltPillar(BlockManager &blocks, int32_t x, int32_t originY, int32_t z,
                                       std::mt19937 &random) {
    const std::string &above = blocks.getBlockAt(x, originY + 1, z).mName;
    if (!isAirAt(blocks, x, originY, z)
        || (above != "minecraft:soul_sand" && above != "minecraft:soul_soil" && above != "minecraft:netherrack"))
        return false;

    float chances[4];
    for (float &chance: chances)
        chance = nextFloat(random);

    // always below the game's 100, but each test draws
    nextFloat(random);
    for (float &chance: chances)
        chance = chance * 0.07999998f + 0.87f;

    int32_t y = originY;
    blocks.setBlockStateAt(x, y, z, basalt());
    static const int32_t SIDES[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    while (true) {
        for (int32_t side = 0; side < 4; side++) {
            if (chances[side] > nextFloat(random))
                placeBasaltIfBacked(blocks, x + SIDES[side][0], y, z + SIDES[side][1]);
        }

        y--;
        nextFloat(random);
        if (!isAirAt(blocks, x, y, z) || isAirAt(blocks, x, y + 1, z))
            break;

        blocks.setBlockStateAt(x, y, z, basalt());
    }

    for (const auto &side: SIDES) {
        if (!isAirAt(blocks, x + side[0], y, z + side[1]))
            blocks.setBlockStateAt(x + side[0], y + 1, z + side[1], basalt());
    }

    for (int32_t dx = -3; dx <= 3; dx++) {
        for (int32_t dz = -3; dz <= 3; dz++) {
            if (nextInt(random, 10) < std::abs(dx * dz))
                continue;

            for (int32_t depth = 0; depth < 3; depth++) {
                if (!isAirAt(blocks, x + dx, y - depth - 1, z + dz)) {
                    blocks.setBlockStateAt(x + dx, y - depth, z + dz, basalt());
                    break;
                }
            }
        }
    }
    return true;
}

bool NetherFeatures::placeDelta(BlockManager &blocks, int32_t x, int32_t originY, int32_t z, std::mt19937 &random) {
    int32_t y = originY;
    if (y < 2)
        return false;

    const int32_t step = isSolidAt(blocks, x, y, z) ? 1 : -1;
    while (true) {
        if (y >= MAX_HEIGHT)
            return false;

        if (isAirAt(blocks, x, y, z)) {
            const BlockState &below = blocks.getBlockAt(x, y - 1, z);
            if (below.mName != "minecraft:bedrock" && !isAir(below) && !isLava(below))
                break;
        }

        y += step;
        if (y <= 1)
            return false;
    }

    const bool rimmed = nextFloat(random) < 0.9f;
    int32_t offsetX = 0;
    int32_t offsetZ = 0;
    if (rimmed) {
        offsetX = nextInt(random, 3);
        offsetZ = nextInt(random, 3);
    }
    const bool hasRim = rimmed && offsetX != 0 && offsetZ != 0;
    const int32_t sizeX = nextInt(random, 4) + 3;
    const int32_t sizeZ = nextInt(random, 4) + 3;
    const int32_t reach = std::max(sizeX, sizeZ);

    bool placed = false;
    for (int32_t dx = -sizeX; dx <= sizeX; dx++) {
        for (int32_t dz = -sizeZ; dz <= sizeZ; dz++) {
            if (std::abs(dx) + std::abs(dz) > reach || !deltaFits(blocks, x + dx, y, z + dz))
                continue;

            if (hasRim) {
                blocks.setBlockStateAt(x + dx, y, z + dz, magma());
                placed = true;
            }
            if (deltaFits(blocks, x + dx + offsetX, y, z + dz + offsetZ)) {
                blocks.setBlockStateAt(x + dx + offsetX, y, z + dz + offsetZ, lava());
                placed = true;
            }
        }
    }
    return placed;
}

bool NetherFeatures::placeBasaltColumns(BlockManager &blocks, int32_t x, int32_t originY, int32_t z,
                                        std::mt19937 &random) {
    int32_t y = originY;
    while (isSolidAt(blocks, x, y, z) || !isSolidAt(blocks, x, y - 1, z)) {
        y--;
        if (y <= MIN_HEIGHT)
            return false;
    }
    if (y <= MIN_HEIGHT || y >= MAX_HEIGHT)
        return false;

    const int32_t tall = nextFloat(random) >= 0.3f ? 1 : 0;
    const int32_t reachBound = tall + 1;
    const int32_t height = nextInt(random, 2 * tall + 4) + 4 * tall + 1;
    const bool clustered = nextFloat(random) < 0.9f;
    const int32_t tries = clustered ? 50 : 15;
    const int32_t spreadBound = clustered ? 11 : 17;
    const int32_t spreadOrigin = clustered ? -5 : -8;

    for (int32_t attempt = 0; attempt < tries; attempt++) {
        const int32_t offsetX = nextInt(random, spreadBound);
        const int32_t offsetZ = nextInt(random, spreadBound);
        const int32_t centerX = x + spreadOrigin + offsetX;
        const int32_t centerZ = z + spreadOrigin + offsetZ;
        const int32_t distance = std::abs(centerX - x) + std::abs(centerZ - z);
        if (height <= distance)
            continue;

        const int32_t reach = nextInt(random, reachBound) + reachBound;
        bool placed = false;
        for (int32_t dx = -reach; dx <= reach && !placed; dx++) {
            for (int32_t dz = -reach; dz <= reach && !placed; dz++) {
                const int32_t away = std::abs(dx) + std::abs(dz);
                placed = placeColumn(blocks, centerX + dx, y, centerZ + dz, away, height - distance - away / 2);
            }
        }
    }
    return true;
}
