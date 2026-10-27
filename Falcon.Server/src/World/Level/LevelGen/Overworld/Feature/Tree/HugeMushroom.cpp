#include "World/Level/LevelGen/Overworld/Feature/Tree/HugeMushroom.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

HugeMushroom::HugeMushroom() : mMushroomType(MushroomType::RANDOM) {
}

HugeMushroom::HugeMushroom(MushroomType mushroomType) : mMushroomType(mushroomType) {
}

bool HugeMushroom::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    MushroomType block = mMushroomType;
    if (block == MushroomType::RANDOM)
        block = random.nextBoolean() ? MushroomType::RED : MushroomType::BROWN;

    const BlockState mushroom = block == MushroomType::BROWN
                                ? VanillaBlocks::BROWN_MUSHROOM_BLOCK().toBlockState()
                                : VanillaBlocks::RED_MUSHROOM_BLOCK().toBlockState();

    int32_t i = random.nextInt(3) + 4;

    if (random.nextInt(12) == 0)
        i *= 2;

    bool flag = true;

    if (y < 1 || y + i + 1 >= 256)
        return false;

    for (int32_t j = y; j <= y + 1 + i; ++j) {
        int32_t k = 3;

        if (j <= y + 3)
            k = 0;

        for (int32_t l = x - k; l <= x + k && flag; ++l) {
            for (int32_t i1 = z - k; i1 <= z + k && flag; ++i1) {
                if (j >= 0 && j < 256) {
                    const BlockState &material = manager.getBlockAt(l, j, i1);
                    if (!VanillaTreeObject::canBeReplaced(material) && !VanillaTreeObject::isLeaves(material.mName))
                        flag = false;
                } else {
                    flag = false;
                }
            }
        }
    }

    if (!flag)
        return false;

    const std::string ground = manager.getBlockAt(x, y - 1, z).mName;
    if (ground != "minecraft:dirt" && ground != "minecraft:grass_block" && ground != "minecraft:mycelium")
        return false;

    int32_t k2 = y + i;

    if (block == MushroomType::RED)
        k2 = y + i - 3;

    for (int32_t l2 = k2; l2 <= y + i; ++l2) {
        int32_t j3 = 1;

        if (l2 < y + i)
            ++j3;

        if (block == MushroomType::BROWN)
            j3 = 3;

        const int32_t k3 = x - j3;
        const int32_t l3 = x + j3;
        const int32_t j1 = z - j3;
        const int32_t k1 = z + j3;

        for (int32_t l1 = k3; l1 <= l3; ++l1) {
            for (int32_t i2 = j1; i2 <= k1; ++i2) {
                int32_t j2 = 5;

                if (l1 == k3)
                    --j2;
                else if (l1 == l3)
                    ++j2;

                if (i2 == j1)
                    j2 -= 3;
                else if (i2 == k1)
                    j2 += 3;

                int32_t meta = j2;

                if (block == MushroomType::BROWN || l2 < y + i) {
                    if ((l1 == k3 || l1 == l3) && (i2 == j1 || i2 == k1))
                        continue;

                    if (l1 == x - (j3 - 1) && i2 == j1)
                        meta = NORTH_WEST;

                    if (l1 == k3 && i2 == z - (j3 - 1))
                        meta = NORTH_WEST;

                    if (l1 == x + (j3 - 1) && i2 == j1)
                        meta = NORTH_EAST;

                    if (l1 == l3 && i2 == z - (j3 - 1))
                        meta = NORTH_EAST;

                    if (l1 == x - (j3 - 1) && i2 == k1)
                        meta = SOUTH_WEST;

                    if (l1 == k3 && i2 == z + (j3 - 1))
                        meta = SOUTH_WEST;

                    if (l1 == x + (j3 - 1) && i2 == k1)
                        meta = SOUTH_EAST;

                    if (l1 == l3 && i2 == z + (j3 - 1))
                        meta = SOUTH_EAST;
                }

                if (meta == CENTER && l2 < y + i)
                    meta = ALL_INSIDE;

                if (y >= y + i - 1 || meta != ALL_INSIDE) {
                    if (!VanillaTreeObject::isSolid(manager.getBlockAt(l1, l2, i2))) {
                        BlockState state = mushroom;
                        state.mStates.putInt("huge_mushroom_bits", meta);
                        manager.setBlockStateAt(l1, l2, i2, BlockState(state.mName, state.mStates));
                    }
                }
            }
        }
    }

    for (int32_t i3 = 0; i3 < i; ++i3) {
        if (!VanillaTreeObject::isSolid(manager.getBlockAt(x, y + i3, z))) {
            BlockState state = mushroom;
            state.mStates.putInt("huge_mushroom_bits", STEM);
            manager.setBlockStateAt(x, y + i3, z, BlockState(state.mName, state.mStates));
        }
    }

    return true;
}
