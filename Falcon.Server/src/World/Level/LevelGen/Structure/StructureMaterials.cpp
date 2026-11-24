#include "World/Level/LevelGen/Structure/StructureMaterials.h"

#include "World/Level/Block/BlockMaterial.h"

bool StructureMaterials::isLiquid(const BlockState &state) {
    return (BlockMaterial::flagsOf(state) & BlockMaterial::FLAG_LIQUID) != 0;
}

bool StructureMaterials::isPlant(const BlockState &state) {
    BlockMaterialType type;
    return BlockMaterial::find(state, type) && type == BlockMaterialType::Plant;
}

bool StructureMaterials::stopsTopSolidSearch(const BlockState &state, bool water, bool leaves) {
    BlockMaterialType type;
    if (!BlockMaterial::find(state, type))
        return false;

    if (type == BlockMaterialType::Leaves)
        return leaves;
    if (water && type == BlockMaterialType::Water)
        return true;
    return (BlockMaterial::flagsOf(type) & BlockMaterial::FLAG_SOLID) != 0;
}

bool StructureMaterials::isBuriedTreasureGround(const BlockState &state) {
    const uint8_t flags = BlockMaterial::flagsOf(state);
    return (flags & (BlockMaterial::FLAG_BIT0 | BlockMaterial::FLAG_SOLID)) == BlockMaterial::FLAG_SOLID
           && BlockMaterial::hasDefaultProperty(state);
}
