#include "World/Level/LevelGen/Nether/Feature/NetherDecoration.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"
#include "World/Level/LevelGen/Random/GameRandom.h"

#include <string>
#include <vector>

namespace {
    using GameRandom::nextInt;

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &netherrackState() {
        static const BlockState state = VanillaBlocks::NETHERRACK().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    const BlockState &glowstoneState() {
        static const BlockState state = VanillaBlocks::GLOWSTONE().toBlockState();
        return state;
    }

    const BlockState &fireState() {
        static const BlockState state = VanillaBlocks::FIRE().toBlockState();
        return state;
    }

    const BlockState &soulFireState() {
        static const BlockState state = VanillaBlocks::SOUL_FIRE().toBlockState();
        return state;
    }

    const BlockState &debrisState() {
        static const BlockState state = VanillaBlocks::ANCIENT_DEBRIS().toBlockState();
        return state;
    }

    const BlockState &brownMushroomState() {
        static const BlockState state = VanillaBlocks::BROWN_MUSHROOM().toBlockState();
        return state;
    }

    const BlockState &redMushroomState() {
        static const BlockState state = VanillaBlocks::RED_MUSHROOM().toBlockState();
        return state;
    }

    bool same(const BlockState &first, const BlockState &second) {
        return first.getHash() == second.getHash();
    }

    bool isName(const BlockState &state, const char *name) {
        return state.mName == name;
    }

    // the difference of two raw draws masked to a range, the game's offset around a feature's position
    int32_t spread(std::mt19937 &random, uint32_t mask) {
        const int32_t first = static_cast<int32_t>(random() & mask);
        return first - static_cast<int32_t>(random() & mask);
    }

    const int32_t FACINGS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};
}

NetherDecoration::NetherDecoration(BlockManager &blocks, const LevelChunk &chunk, uint32_t levelSeed)
        : mBlocks(blocks), mRandom(levelSeed), mBaseX(chunk.getX() << 4), mBaseZ(chunk.getZ() << 4) {
    const uint32_t multiplierX = static_cast<uint32_t>(nextInt(mRandom)) | 1u;
    const uint32_t multiplierZ = static_cast<uint32_t>(nextInt(mRandom)) | 1u;
    mDecorationSeed = (multiplierZ * static_cast<uint32_t>(chunk.getZ())
                       + multiplierX * static_cast<uint32_t>(chunk.getX())) ^ levelSeed;
    mRandom.seed(mDecorationSeed);
}

int32_t NetherDecoration::_at(int32_t origin, int32_t bound) {
    return origin + nextInt(mRandom, bound);
}

void NetherDecoration::placeBeforeBiomes() {
    const int32_t centerX = mBaseX | 8;
    const int32_t centerZ = mBaseZ | 8;

    for (int32_t i = 0; i < 8; i++) {
        const int32_t x = _at(centerX, 16);
        const int32_t y = _at(4, 120);
        const int32_t z = _at(centerZ, 16);
        _placeSpring(x, y, z, false);
    }

    const int32_t fires = nextInt(mRandom, nextInt(mRandom, 10) + 1);
    for (int32_t i = 0; i <= fires; i++) {
        const int32_t x = _at(centerX, 16);
        const int32_t y = _at(4, 120);
        const int32_t z = _at(centerZ, 16);
        _placeFire(x, y, z);
    }

    const int32_t glowstones = nextInt(mRandom, nextInt(mRandom, 10) + 1);
    for (int32_t i = 0; i < glowstones; i++) {
        const int32_t x = _at(centerX, 16);
        const int32_t y = _at(4, 120);
        const int32_t z = _at(centerZ, 16);
        _placeGlowstone(x, y, z);
    }

    for (int32_t i = 0; i < 10; i++) {
        const int32_t x = _at(centerX, 16);
        const int32_t y = _at(0, 128);
        const int32_t z = _at(centerZ, 16);
        _placeGlowstone(x, y, z);
    }

    for (const BlockState *mushroom: {&brownMushroomState(), &redMushroomState()}) {
        // always zero, but the draw moves the random
        if (nextInt(mRandom, 1) != 0)
            continue;

        const int32_t x = _at(centerX, 16);
        const int32_t y = _at(0, 128);
        const int32_t z = _at(centerZ, 16);
        _placeMushrooms(x, y, z, *mushroom);
    }

    const std::vector<std::string> netherrack = {"minecraft:netherrack"};
    const auto veins = [&](int32_t count, int32_t bottom, int32_t range, int32_t size, Block (*places)(),
                           const std::vector<std::string> &mayReplace) {
        for (int32_t i = 0; i < count; i++) {
            const int32_t x = _at(mBaseX, 16);
            const int32_t y = _at(bottom, range);
            const int32_t z = _at(mBaseZ, 16);
            OverworldRulePlacer::placeOreVein(mBlocks, size, places, mayReplace, x, y, z, mRandom);
        }
    };

    veins(10, 10, 108, 10, &VanillaBlocks::NETHER_GOLD_ORE, netherrack);
    veins(16, 10, 108, 14, &VanillaBlocks::QUARTZ_ORE, netherrack);

    for (int32_t i = 0; i < 16; i++) {
        const int32_t x = _at(mBaseX, 16);
        const int32_t y = _at(10, 108);
        const int32_t z = _at(mBaseZ, 16);
        _placeSpring(x, y, z, true);
    }

    veins(9, 23, 14, 28, &VanillaBlocks::MAGMA, {"minecraft:netherrack", "minecraft:blackstone"});
    veins(2, 5, 32, 33, &VanillaBlocks::GRAVEL, netherrack);
    veins(2, 5, 32, 33, &VanillaBlocks::BLACKSTONE, netherrack);

    mRandom.seed(mDecorationSeed);
}

void NetherDecoration::placeAfterBiomes() {
    for (int32_t i = 0; i < 2; i++) {
        const int32_t x = _at(mBaseX, 16);
        const int32_t y = _at(8, 16);
        const int32_t z = _at(mBaseZ, 16);
        _placeDebris(x, y, z, 4);
    }

    for (int32_t i = 0; i < 3; i++) {
        const int32_t x = _at(mBaseX, 16);
        const int32_t y = _at(8, 112);
        const int32_t z = _at(mBaseZ, 16);
        _placeDebris(x, y, z, 3);
    }
}

void NetherDecoration::_placeSpring(int32_t x, int32_t y, int32_t z, bool closed) {
    // sub_14D22B700
    if (!same(mBlocks.getBlockAt(x, y + 1, z), netherrackState()))
        return;

    const BlockState &here = mBlocks.getBlockAt(x, y, z);
    if (!same(here, airState()) && !same(here, netherrackState()))
        return;

    int32_t rock = 0;
    int32_t air = 0;
    const int32_t sides[5][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, -1, 0}};
    for (const int32_t *side: sides) {
        const BlockState &state = mBlocks.getBlockAt(x + side[0], y + side[1], z + side[2]);
        rock += same(state, netherrackState());
        air += same(state, airState());
    }

    if ((!closed && rock == 4 && air == 1) || rock == 5)
        mBlocks.setBlockStateAt(x, y, z, lavaState());
}

void NetherDecoration::_placeFire(int32_t x, int32_t y, int32_t z) {
    // sub_14D22A340
    for (int32_t i = 0; i < 64; i++) {
        const int32_t offsetZ = spread(mRandom, 7);
        const int32_t offsetY = spread(mRandom, 3);
        const int32_t offsetX = spread(mRandom, 7);
        const int32_t fireX = x + offsetX;
        const int32_t fireY = y + offsetY;
        const int32_t fireZ = z + offsetZ;
        if (!same(mBlocks.getBlockAt(fireX, fireY, fireZ), airState()))
            continue;

        const BlockState &below = mBlocks.getBlockAt(fireX, fireY - 1, fireZ);
        if (same(below, netherrackState()))
            mBlocks.setBlockStateAt(fireX, fireY, fireZ, fireState());
        else if (isName(below, "minecraft:soul_soil"))
            mBlocks.setBlockStateAt(fireX, fireY, fireZ, soulFireState());
    }
}

void NetherDecoration::_placeGlowstone(int32_t x, int32_t y, int32_t z) {
    // sub_14D213320
    if (!same(mBlocks.getBlockAt(x, y, z), airState()))
        return;

    const BlockState &above = mBlocks.getBlockAt(x, y + 1, z);
    if (!same(above, netherrackState()) && !isName(above, "minecraft:basalt") && !isName(above, "minecraft:blackstone"))
        return;

    mBlocks.setBlockStateAt(x, y, z, glowstoneState());
    for (int32_t i = 0; i < 1500; i++) {
        const int32_t offsetZ = spread(mRandom, 7);
        const int32_t offsetY = nextInt(mRandom, 12);
        const int32_t offsetX = spread(mRandom, 7);
        const int32_t blockX = x + offsetX;
        const int32_t blockY = y - offsetY;
        const int32_t blockZ = z + offsetZ;
        if (!same(mBlocks.getBlockAt(blockX, blockY, blockZ), airState()))
            continue;

        int32_t neighbours = 0;
        for (const int32_t *facing: {FACINGS[4], FACINGS[5], FACINGS[2], FACINGS[3], FACINGS[0], FACINGS[1]})
            neighbours += same(mBlocks.getBlockAt(blockX + facing[0], blockY + facing[1], blockZ + facing[2]),
                               glowstoneState());

        if (neighbours == 1)
            mBlocks.setBlockStateAt(blockX, blockY, blockZ, glowstoneState());
    }
}

void NetherDecoration::_placeMushrooms(int32_t x, int32_t y, int32_t z, const BlockState &mushroom) {
    // sub_14D2261D0 always draws its 64 tries, whatever they find
    int32_t blockX = x;
    int32_t blockY = y;
    int32_t blockZ = z;
    for (int32_t i = 0; i < 64; i++) {
        if (same(mBlocks.getBlockAt(blockX, blockY, blockZ), airState())
            && BlockSupport::isSolid(mBlocks.getBlockAt(blockX, blockY - 1, blockZ)))
            mBlocks.setBlockStateAt(blockX, blockY, blockZ, mushroom);

        const int32_t offsetZ = spread(mRandom, 7);
        const int32_t offsetY = spread(mRandom, 3);
        const int32_t offsetX = spread(mRandom, 7);
        blockX = x + offsetX;
        blockY = y + offsetY;
        blockZ = z + offsetZ;
    }
}

void NetherDecoration::_placeDebris(int32_t x, int32_t y, int32_t z, int32_t maxCount) {
    // sub_14D22BB00, the scatter ore of nether_netherite_deep_feature and nether_netherite_tall_feature
    const int32_t count = nextInt(mRandom, maxCount);
    for (int32_t i = 0; i < count; i++) {
        int32_t blockX = x;
        int32_t blockY = y;
        int32_t blockZ = z;
        for (int32_t walk = i == 0 ? 0 : i * 2; walk > 0; walk--) {
            const int32_t *facing = FACINGS[nextInt(mRandom, 6)];
            blockX += facing[0];
            blockY += facing[1];
            blockZ += facing[2];
        }

        const int64_t dx = blockX - x;
        const int64_t dy = blockY - y;
        const int64_t dz = blockZ - z;
        if (dx * dx + dy * dy + dz * dz > 36)
            continue;

        if (!same(mBlocks.getBlockAt(blockX, blockY, blockZ), netherrackState()))
            continue;

        bool exposed = false;
        for (const auto &facing: FACINGS)
            exposed = exposed || same(mBlocks.getBlockAt(blockX + facing[0], blockY + facing[1], blockZ + facing[2]),
                                      airState());

        if (!exposed)
            mBlocks.setBlockStateAt(blockX, blockY, blockZ, debrisState());
    }
}
