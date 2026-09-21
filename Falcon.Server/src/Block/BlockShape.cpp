#include "Block/BlockShape.h"

#include "Block/BlockData.h"
#include "Block/Blocks/VanillaBlocks.h"

#include <algorithm>
#include <string>

namespace {

    const AxisAlignedBB EMPTY_SHAPE(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    const AxisAlignedBB FULL_CUBE(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

    const float FENCE_MIN = 0.375f;
    const float FENCE_MAX = 0.625f;
    const float FENCE_HEIGHT = 1.5f;
    const float THIN_MIN = 7.0f / 16.0f;
    const float THIN_MAX = 9.0f / 16.0f;

    bool endsWith(const std::string &value, const char *suffix) {
        const std::string tail(suffix);
        if (value.size() < tail.size())
            return false;

        return value.compare(value.size() - tail.size(), tail.size(), tail) == 0;
    }

    bool isUpsideDown(const BlockState &state) {
        return state.mStates.getInt("upside_down_bit", 0) != 0;
    }

    bool isHanging(const BlockState &state) {
        return state.mStates.getInt("hanging", 0) != 0;
    }

}

bool BlockShape::hasCollision(const BlockState &state) {
    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return false;

    return data->mSolid;
}

bool BlockShape::isFullCube(const BlockState &state) {
    const AxisAlignedBB shape = getRelativeShape(state);

    return shape.mMinX <= 0.0f && shape.mMinY <= 0.0f && shape.mMinZ <= 0.0f
           && shape.mMaxX >= 1.0f && shape.mMaxY >= 1.0f && shape.mMaxZ >= 1.0f;
}

AxisAlignedBB BlockShape::getRelativeShape(const BlockState &state) {
    if (!hasCollision(state))
        return EMPTY_SHAPE;

    const std::string &name = state.mName;

    const Block *block = VanillaBlocks::fromIdentifier(name);
    AxisAlignedBB shape = FULL_CUBE;
    if (block != nullptr && block->getCollisionShape(state, shape))
        return shape;

    if (endsWith(name, "_stairs")) {
        if (isUpsideDown(state))
            return AxisAlignedBB(0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f);

        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f);
    }

    if (endsWith(name, "_fence") || endsWith(name, "_wall") || endsWith(name, "_fence_gate"))
        return AxisAlignedBB(FENCE_MIN, 0.0f, FENCE_MIN, FENCE_MAX, FENCE_HEIGHT, FENCE_MAX);

    if (endsWith(name, "_pane") || name == "minecraft:iron_bars" || name == "minecraft:glass_pane")
        return AxisAlignedBB(THIN_MIN, 0.0f, THIN_MIN, THIN_MAX, 1.0f, THIN_MAX);

    if (endsWith(name, "_carpet") || name == "minecraft:carpet" || name == "minecraft:moss_carpet")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.0625f, 1.0f);

    if (name == "minecraft:cactus")
        return AxisAlignedBB(0.0625f, 0.0f, 0.0625f, 0.9375f, 0.9375f, 0.9375f);

    if (name == "minecraft:chest" || name == "minecraft:trapped_chest" || name == "minecraft:ender_chest")
        return AxisAlignedBB(0.0625f, 0.0f, 0.0625f, 0.9375f, 0.9475f, 0.9375f);

    if (name == "minecraft:brewing_stand")
        return AxisAlignedBB(7.0f / 16.0f, 0.0f, 7.0f / 16.0f, 1.0f - 7.0f / 16.0f, 1.0f - 2.0f / 16.0f,
                             1.0f - 7.0f / 16.0f);

    if (name == "minecraft:conduit")
        return AxisAlignedBB(5.0f / 16.0f, 5.0f / 16.0f, 5.0f / 16.0f, 11.0f / 16.0f, 11.0f / 16.0f,
                             11.0f / 16.0f);

    if (name == "minecraft:daylight_detector" || name == "minecraft:daylight_detector_inverted")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.625f, 1.0f);

    if (name == "minecraft:enchanting_table")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 12.0f / 16.0f, 1.0f);

    if (name == "minecraft:end_portal_frame") {
        const float height = state.mStates.getInt("end_portal_eye_bit", 0) != 0 ? 1.0f : 0.8125f;
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, height, 1.0f);
    }

    if (name == "minecraft:flower_pot")
        return AxisAlignedBB(0.3125f, 0.0f, 0.3125f, 0.6875f, 0.375f, 0.6875f);

    if (name == "minecraft:heavy_core")
        return AxisAlignedBB(0.25f, 0.0f, 0.25f, 0.75f, 0.5f, 0.75f);

    if (name == "minecraft:honey_block")
        return AxisAlignedBB(0.1f, 0.0f, 0.1f, 0.9f, 1.0f, 0.9f);

    if (name == "minecraft:chain")
        return AxisAlignedBB(7.0f / 16.0f, 0.0f, 7.0f / 16.0f, 9.0f / 16.0f, 1.0f, 9.0f / 16.0f);

    if (name == "minecraft:end_rod")
        return AxisAlignedBB(0.4f, 0.0f, 0.4f, 0.6f, 1.0f, 0.6f);

    if (name == "minecraft:lantern" || name == "minecraft:soul_lantern") {
        const bool hanging = isHanging(state);
        const float minY = hanging ? 1.0f / 16.0f : 0.0f;
        const float maxY = hanging ? 8.0f / 16.0f : 7.0f / 16.0f;
        return AxisAlignedBB(5.0f / 16.0f, minY, 5.0f / 16.0f, 11.0f / 16.0f, maxY, 11.0f / 16.0f);
    }

    if (name == "minecraft:lectern")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.89999f, 1.0f);

    if (name == "minecraft:stonecutter_block")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 9.0f / 16.0f, 1.0f);

    if (name == "minecraft:campfire" || name == "minecraft:soul_campfire")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.4371948f, 1.0f);

    if (name == "minecraft:bed")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.5625f, 1.0f);

    if (name == "minecraft:cake") {
        const int32_t bites = state.mStates.getInt("bite_counter", 0);
        return AxisAlignedBB((1.0f + (float) bites * 2.0f) / 16.0f, 0.0f, 0.0625f, 0.9375f, 0.5f, 0.9375f);
    }

    if (name == "minecraft:snow_layer") {
        const int32_t height = state.mStates.getInt("height", 0);
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f,
                             (float) (std::min(16, height + 1) * 2) / 16.0f, 1.0f);
    }

    if (name == "minecraft:scaffolding")
        return AxisAlignedBB(0.0f, 14.0f / 16.0f, 0.0f, 1.0f, 1.0f, 1.0f);

    if (name == "minecraft:turtle_egg")
        return AxisAlignedBB(3.0f / 16.0f, 0.0f, 3.0f / 16.0f, 12.0f / 16.0f, 7.0f / 16.0f, 12.0f / 16.0f);

    if (name == "minecraft:sniffer_egg")
        return AxisAlignedBB(1.0f / 16.0f, 0.0f, 1.0f / 16.0f, 15.0f / 16.0f, 1.0f, 15.0f / 16.0f);

    if (name == "minecraft:waterlily")
        return AxisAlignedBB(0.0625f, 0.0f, 0.0625f, 0.9375f, 0.015625f, 0.9375f);

    if (name == "minecraft:bamboo_sapling")
        return AxisAlignedBB(0.125f, 0.0f, 0.125f, 0.875f, 0.875f, 0.875f);

    if (endsWith(name, "_pressure_plate"))
        return AxisAlignedBB(0.625f, 0.0f, 0.625f, 0.9375f, 0.0625f, 0.9375f);

    if (name == "minecraft:rail" || name == "minecraft:golden_rail" || name == "minecraft:detector_rail"
        || name == "minecraft:activator_rail")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.125f, 1.0f);

    if (name == "minecraft:unpowered_repeater" || name == "minecraft:powered_repeater"
        || name == "minecraft:unpowered_comparator" || name == "minecraft:powered_comparator")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.125f, 1.0f);

    if (name == "minecraft:trip_wire")
        return AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f);

    return FULL_CUBE;
}

AxisAlignedBB BlockShape::getShapeAt(const BlockState &state, int32_t x, int32_t y, int32_t z) {
    const AxisAlignedBB shape = getRelativeShape(state);
    if (shape.isEmpty())
        return shape;

    return shape.offset((float) x, (float) y, (float) z);
}

bool BlockShape::isPositionInside(const BlockState &state, int32_t blockX, int32_t blockY, int32_t blockZ,
                                  float x, float y, float z) {
    const AxisAlignedBB shape = getShapeAt(state, blockX, blockY, blockZ);
    if (shape.isEmpty())
        return false;

    return shape.isVectorInside(x, y, z);
}
