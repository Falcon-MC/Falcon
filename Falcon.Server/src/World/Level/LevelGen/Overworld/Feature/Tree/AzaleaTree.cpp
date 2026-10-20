#include "World/Level/LevelGen/Overworld/Feature/Tree/AzaleaTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

bool AzaleaTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t i = random.nextInt(2) + 2;
    const int32_t i2 = y + i;

    if (y < -63 || y + i + 2 >= 320)
        return false;

    for (int32_t il = 0; il < i + 1; il++)
        _placeLogAt(manager, x, il + y, z);

    setDirtAt(manager, x, y - 1, z);

    for (int32_t i3 = -2; i3 <= 1; ++i3) {
        for (int32_t l3 = -2; l3 <= 1; ++l3) {
            int32_t k4 = 1;
            int32_t offsetX = random.nextInt(0, 1);
            int32_t offsetY = random.nextInt(0, 1);
            int32_t offsetZ = random.nextInt(0, 1);
            _placeLeafAt(manager, x + i3 + offsetX, i2 + k4 + offsetY, z + l3 + offsetZ, random);
            _placeLeafAt(manager, x - i3 + offsetX, i2 + k4 + offsetY, z + l3 + offsetZ, random);
            _placeLeafAt(manager, x + i3 + offsetX, i2 + k4 + offsetY, z - l3 + offsetZ, random);
            _placeLeafAt(manager, x - i3 + offsetX, i2 + k4 + offsetY, z - l3 + offsetZ, random);

            k4 = 0;
            _placeLeafAt(manager, x + i3, i2 + k4, z + l3, random);
            _placeLeafAt(manager, x - i3, i2 + k4, z + l3, random);
            _placeLeafAt(manager, x + i3, i2 + k4, z - l3, random);
            _placeLeafAt(manager, x - i3, i2 + k4, z - l3, random);

            k4 = 1;
            _placeLeafAt(manager, x + i3, i2 + k4, z + l3, random);
            _placeLeafAt(manager, x - i3, i2 + k4, z + l3, random);
            _placeLeafAt(manager, x + i3, i2 + k4, z - l3, random);
            _placeLeafAt(manager, x - i3, i2 + k4, z - l3, random);

            k4 = 2;
            offsetX = random.nextInt(-1, 0);
            offsetY = random.nextInt(-1, 0);
            offsetZ = random.nextInt(-1, 0);

            _placeLeafAt(manager, x + i3 + offsetX, i2 + k4 + offsetY, z + l3 + offsetZ, random);
            _placeLeafAt(manager, x - i3 + offsetX, i2 + k4 + offsetY, z + l3 + offsetZ, random);
            _placeLeafAt(manager, x + i3 + offsetX, i2 + k4 + offsetY, z - l3 + offsetZ, random);
            _placeLeafAt(manager, x - i3 + offsetX, i2 + k4 + offsetY, z - l3 + offsetZ, random);
        }
    }

    return true;
}

void AzaleaTree::setDirtAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    manager.setBlockStateAt(x, y, z, VanillaBlocks::DIRT_WITH_ROOTS().toBlockState());
}

void AzaleaTree::_placeLogAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    const std::string identifier = manager.getBlockAt(x, y, z).mName;

    if (identifier == "minecraft:air" || identifier == "minecraft:azalea_leaves"
        || identifier == "minecraft:azalea_leaves_flowered")
        manager.setBlockStateAt(x, y, z, withPillarAxis(VanillaBlocks::OAK_LOG().toBlockState(), "y"));
}

void AzaleaTree::_placeLeafAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    const std::string identifier = manager.getBlockAt(x, y, z).mName;

    if (identifier == "minecraft:air") {
        if (random.nextInt(3) == 1)
            manager.setBlockStateAt(x, y, z, VanillaBlocks::AZALEA_LEAVES_FLOWERED().toBlockState());
        else
            manager.setBlockStateAt(x, y, z, VanillaBlocks::AZALEA_LEAVES().toBlockState());
    }
}
