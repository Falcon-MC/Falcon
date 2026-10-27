#include "World/Level/LevelGen/End/Tree/LegacyChorusTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBlockFace.h"
#include "World/Level/Block/ChorusPlantBlock.h"
#include "World/Level/Block/Components/PlacementOrientation.h"

#include <cmath>
#include <cstdlib>

namespace {

    BlockState chorusPlantState() {
        return VanillaBlocks::CHORUS_PLANT().toBlockState();
    }

    BlockState chorusFlowerFullyAgedState() {
        BlockState state = VanillaBlocks::CHORUS_FLOWER().toBlockState();
        state.mStates.putInt("age", 5);
        return BlockState(state.mName, state.mStates);
    }

}

bool LegacyChorusTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    return generate(manager, random, x, y, z, 8);
}

bool LegacyChorusTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z,
                                int32_t maxSize) {
    manager.setBlockStateAt(x, y, z, chorusPlantState());
    std::vector<Vector3i> plants{Vector3i(x, y, z)};
    _grow(manager, random, x, y, z, maxSize, 0, plants);
    _connect(manager, plants);
    return true;
}

void LegacyChorusTree::growImmediately(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z,
                                       int32_t maxSize, int32_t age) {
    std::vector<Vector3i> plants;
    _grow(manager, random, x, y, z, maxSize, age, plants);
    _connect(manager, plants);
}

void LegacyChorusTree::_connect(BlockManager &manager, const std::vector<Vector3i> &plants) {
    for (const Vector3i &position: plants) {
        const BlockState state = manager.getBlockAt(position.x, position.y, position.z);
        manager.setBlockStateAt(position.x, position.y, position.z,
                               ChorusPlantBlock::connectedState(state, [&manager, &position](int face) {
            const Vector3i other = PlacementOrientation::relativePosition(position, face);
            return &manager.getBlockAt(other.x, other.y, other.z);
        }));
    }
}

void LegacyChorusTree::_grow(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z,
                             int32_t maxSize, int32_t age, std::vector<Vector3i> &plants) {
    int32_t height = 1 + random.nextInt(4);
    if (age == 0)
        height++;

    for (int32_t offsetY = 1; offsetY <= height; offsetY++) {
        if (!_isHorizontalAir(manager, x, y + offsetY, z))
            return;

        manager.setBlockStateAt(x, y + offsetY, z, chorusPlantState());
        plants.emplace_back(x, y + offsetY, z);
    }

    if (age < 4) {
        int32_t attempt = random.nextInt(4);
        if (age == 0)
            attempt++;

        for (int32_t i = 0; i < attempt; i++) {
            const TreeBlockFace face = TreeBlockFaces::HORIZONTAL_PLANE[random.nextInt(4)];
            const int32_t checkX = x + TreeBlockFaces::getXOffset(face);
            const int32_t checkY = y + height + TreeBlockFaces::getYOffset(face);
            const int32_t checkZ = z + TreeBlockFaces::getZOffset(face);

            if (manager.getBlockAt(checkX, checkY, checkZ).mName == "minecraft:air"
                && manager.getBlockAt(checkX, checkY - 1, checkZ).mName == "minecraft:air") {
                if (std::abs(checkX - x) < maxSize && std::abs(checkZ - z) < maxSize
                    && _isHorizontalAirExcept(manager, checkX, checkY, checkZ,
                                              (int32_t) TreeBlockFaces::getOpposite(face))) {
                    manager.setBlockStateAt(checkX, checkY, checkZ, chorusPlantState());
                    plants.emplace_back(checkX, checkY, checkZ);
                    _grow(manager, random, checkX, checkY, checkZ, maxSize, age + 1, plants);
                }
            }
        }
    } else {
        manager.setBlockStateAt(x, y + height, z, chorusFlowerFullyAgedState());
    }
}

bool LegacyChorusTree::_isHorizontalAir(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    for (const TreeBlockFace face: TreeBlockFaces::HORIZONTAL_PLANE) {
        const int32_t sideX = x + TreeBlockFaces::getXOffset(face);
        const int32_t sideY = y + TreeBlockFaces::getYOffset(face);
        const int32_t sideZ = z + TreeBlockFaces::getZOffset(face);
        if (manager.getBlockAt(sideX, sideY, sideZ).mName != "minecraft:air")
            return false;
    }

    return true;
}

bool LegacyChorusTree::_isHorizontalAirExcept(BlockManager &manager, int32_t x, int32_t y, int32_t z,
                                              int32_t exceptFace) {
    for (const TreeBlockFace face: TreeBlockFaces::HORIZONTAL_PLANE) {
        if ((int32_t) face == exceptFace)
            continue;

        const int32_t sideX = x + TreeBlockFaces::getXOffset(face);
        const int32_t sideY = y + TreeBlockFaces::getYOffset(face);
        const int32_t sideZ = z + TreeBlockFaces::getZOffset(face);
        if (manager.getBlockAt(sideX, sideY, sideZ).mName != "minecraft:air")
            return false;
    }

    return true;
}
