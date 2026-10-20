#include "World/Level/LevelGen/Overworld/Feature/Tree/BigSpruceTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/IFeature.h"

#include <vector>

namespace {

    const std::vector<std::vector<int32_t>> &foliages() {
        static const std::vector<std::vector<int32_t>> table = {
                {1, 0, 0, 1, 2, 1, 1, 2, 3, 2, 2, 3, 4, 3},
                {1, 0, 1, 2, 1, 2, 1, 1, 2, 3, 2, 2, 3, 4, 3},
                {1, 2, 3},
                {1, 2, 1, 3, 2, 4, 3}
        };
        return table;
    }

}

bool BigSpruceTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t height = 24 + random.nextInt(8);
    const int32_t midX = x + 1;
    const int32_t midZ = z + 1;

    if (y < 1 || y + height + 5 >= 256)
        return false;

    const int32_t rad = 6;
    for (int32_t offsetX = -rad - 1; offsetX <= rad; offsetX++) {
        for (int32_t offsetZ = -rad - 1; offsetZ <= rad; offsetZ++) {
            const float calcX = (float) offsetX + 0.5f;
            const float calcZ = (float) offsetZ + 0.5f;
            const float calcRad = (float) rad + 0.8f;
            const int32_t px = midX + offsetX;
            const int32_t pz = midZ + offsetZ;
            if ((calcX * calcX) + (calcZ * calcZ) < (calcRad * calcRad))
                _placePodzolAt(manager, px, manager.getHeightAt(px, pz), pz);
        }
    }

    const std::string ground = manager.getBlockAt(x, y - 1, z).mName;
    if (ground != "minecraft:grass_block" && ground != "minecraft:dirt" && ground != "minecraft:podzol")
        return false;

    const std::vector<int32_t> &leafRadii = foliages()[(size_t) random.nextInt((int32_t) foliages().size() - 1)];

    _placeLeafAt(manager, x, y + height + 1, z);
    _placeLeafAt(manager, x + 1, y + height + 1, z);
    _placeLeafAt(manager, x, y + height + 1, z + 1);
    _placeLeafAt(manager, x + 1, y + height + 1, z + 1);

    for (int32_t offsetY = height; offsetY >= 0; offsetY--) {
        _placeLogAt(manager, x, y + offsetY, z);
        _placeLogAt(manager, x + 1, y + offsetY, z);
        _placeLogAt(manager, x, y + offsetY, z + 1);
        _placeLogAt(manager, x + 1, y + offsetY, z + 1);

        const int32_t index = height - offsetY;
        if (index < (int32_t) leafRadii.size()) {
            const int32_t radius = leafRadii[(size_t) index];
            for (int32_t offsetX = -radius - 1; offsetX <= radius; offsetX++) {
                for (int32_t offsetZ = -radius - 1; offsetZ <= radius; offsetZ++) {
                    const float calcX = (float) offsetX + 0.5f;
                    const float calcZ = (float) offsetZ + 0.5f;
                    const float calcRad = (float) radius + 0.7f;
                    if ((calcX * calcX) + (calcZ * calcZ) < (calcRad * calcRad))
                        _placeLeafAt(manager, midX + offsetX, y + offsetY, midZ + offsetZ);
                }
            }
        }
    }

    return true;
}

void BigSpruceTree::_placeLogAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (canGrowInto(manager.getBlockAt(x, y, z).mName))
        manager.setBlockStateAt(x, y, z, VanillaBlocks::SPRUCE_LOG().toBlockState());
}

void BigSpruceTree::_placeLeafAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    const std::string identifier = manager.getBlockAt(x, y, z).mName;
    if (identifier == "minecraft:air" || identifier == "minecraft:snow_layer")
        manager.setBlockStateAt(x, y, z, VanillaBlocks::SPRUCE_LEAVES().toBlockState());
}

void BigSpruceTree::_placePodzolAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (IFeature::isSupportDirt(manager.getBlockAt(x, y, z)))
        manager.setBlockStateAt(x, y, z, VanillaBlocks::PODZOL().toBlockState());
}
