#include "World/Level/Block/BlockShape.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Level/Block/BlockCollisionShapes.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/VanillaBlocks.h"

#include <algorithm>
#include <string>

namespace {

    const AxisAlignedBB EMPTY_SHAPE(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    const AxisAlignedBB FULL_CUBE(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

}

bool BlockShape::hasCollision(const BlockState &state, const Actor *actor) {
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    AxisAlignedBB shape = FULL_CUBE;
    // a class shape with no height lets actors through
    if (block != nullptr && block->getCollisionShape(state, actor, shape))
        return shape.mMaxY > shape.mMinY;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return false;

    return data->mSolid;
}

bool BlockShape::isFullCube(const BlockState &state) {
    const AxisAlignedBB shape = getRelativeShape(state);

    return shape.mMinX <= 0.0f && shape.mMinY <= 0.0f && shape.mMinZ <= 0.0f && shape.mMaxX >= 1.0f
           && shape.mMaxY >= 1.0f && shape.mMaxZ >= 1.0f;
}

namespace {
    AxisAlignedBB unionOf(const std::vector<AxisAlignedBB> &boxes) {
        if (boxes.empty())
            return EMPTY_SHAPE;
        AxisAlignedBB merged = boxes[0];
        for (const AxisAlignedBB &box: boxes) {
            merged.mMinX = std::min(merged.mMinX, box.mMinX);
            merged.mMinY = std::min(merged.mMinY, box.mMinY);
            merged.mMinZ = std::min(merged.mMinZ, box.mMinZ);
            merged.mMaxX = std::max(merged.mMaxX, box.mMaxX);
            merged.mMaxY = std::max(merged.mMaxY, box.mMaxY);
            merged.mMaxZ = std::max(merged.mMaxZ, box.mMaxZ);
        }
        return merged;
    }

    // the class shape first: it can depend on the actor or the neighbours; then the game's table
    std::vector<AxisAlignedBB> vanillaBoxes(const BlockState &state) {
        if (const std::vector<AxisAlignedBB> *boxes = BlockCollisionShapes::find(state))
            return *boxes;
        // a block of a pack, a script or a plugin: its minecraft:collision_box, in pixels from the bottom center
        if (const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(state.mName)) {
            if (!custom->mCollision.mEnabled)
                return {};
            const CustomBlockBox &box = custom->mCollision;
            return {AxisAlignedBB((box.mOrigin[0] + 8.0f) / 16.0f, box.mOrigin[1] / 16.0f,
                                  (box.mOrigin[2] + 8.0f) / 16.0f, (box.mOrigin[0] + 8.0f + box.mSize[0]) / 16.0f,
                                  (box.mOrigin[1] + box.mSize[1]) / 16.0f,
                                  (box.mOrigin[2] + 8.0f + box.mSize[2]) / 16.0f)};
        }
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        if (data == nullptr || !data->mSolid)
            return {};
        return {FULL_CUBE};
    }
}

// local 0..1 coordinates, one box covering every box of the state
AxisAlignedBB BlockShape::getRelativeShape(const BlockState &state, const Actor *actor) {
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    AxisAlignedBB shape = FULL_CUBE;
    if (block != nullptr && block->getCollisionShape(state, actor, shape))
        return shape.mMaxY > shape.mMinY ? shape : EMPTY_SHAPE;
    return unionOf(vanillaBoxes(state));
}

void BlockShape::appendShapesAt(const BlockState &state, int32_t x, int32_t y, int32_t z, const Actor *actor,
                                const Neighbours &neighbours, std::vector<AxisAlignedBB> &out) {
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    std::vector<AxisAlignedBB> boxes;
    if (block == nullptr || !block->getCollisionShapesAt(state, Vector3i(x, y, z), actor, neighbours, boxes)) {
        AxisAlignedBB placed = FULL_CUBE;
        if (block != nullptr && block->getCollisionShapeAt(state, Vector3i(x, y, z), actor, placed)) {
            if (placed.mMaxY > placed.mMinY)
                boxes.push_back(placed);
        } else {
            boxes = vanillaBoxes(state);
        }
    }
    for (const AxisAlignedBB &box: boxes) {
        if (box.mMaxY > box.mMinY)
            out.push_back(box.offset((float) x, (float) y, (float) z));
    }
}

AxisAlignedBB BlockShape::getShapeAt(const BlockState &state, int32_t x, int32_t y, int32_t z, const Actor *actor) {
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    AxisAlignedBB placed = FULL_CUBE;
    if (block != nullptr && block->getCollisionShapeAt(state, Vector3i(x, y, z), actor, placed)) {
        if (placed.mMaxY <= placed.mMinY)
            return EMPTY_SHAPE;
        return placed.offset((float) x, (float) y, (float) z);
    }

    const AxisAlignedBB shape = getRelativeShape(state, actor);
    if (shape.isEmpty())
        return shape;

    return shape.offset((float) x, (float) y, (float) z);
}

bool BlockShape::isPositionInside(const BlockState &state, int32_t blockX, int32_t blockY, int32_t blockZ, float x,
                                  float y, float z, const Actor *actor) {
    const AxisAlignedBB shape = getShapeAt(state, blockX, blockY, blockZ, actor);
    if (shape.isEmpty())
        return false;

    return shape.isVectorInside(x, y, z);
}
