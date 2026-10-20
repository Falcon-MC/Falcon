#include "World/Level/Block/Actor/PistonArmBlockActor.h"

#include <algorithm>
#include <utility>

namespace {
    const float MOVE_STEP = 0.5f;

    const char *TAG_STATE = "State";
    const char *TAG_NEW_STATE = "NewState";
    const char *TAG_PROGRESS = "Progress";
    const char *TAG_LAST_PROGRESS = "LastProgress";
    const char *TAG_STICKY = "Sticky";
    const char *TAG_EXTENDING = "Extending";
    const char *TAG_FACING = "facing";
    const char *TAG_ATTACHED_BLOCKS = "AttachedBlocks";
    const char *TAG_BREAK_BLOCKS = "BreakBlocks";
    const char *TAG_IS_MOVABLE = "isMovable";

    Tag attachedBlocksTag(const std::vector<Vector3i> &blocks) {
        std::vector<Tag> values;
        values.reserve(blocks.size() * 3);

        for (const Vector3i &block: blocks) {
            values.push_back(Tag::ofInt(block.x));
            values.push_back(Tag::ofInt(block.y));
            values.push_back(Tag::ofInt(block.z));
        }

        return Tag::ofList(Tag::Type::Int, std::move(values));
    }
}

Tag PistonArmBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putByte(TAG_STATE, mState);
    data.putByte(TAG_NEW_STATE, mNewState);
    data.putFloat(TAG_PROGRESS, mProgress);
    data.putFloat(TAG_LAST_PROGRESS, mLastProgress);
    data.putByte(TAG_STICKY, mSticky ? 1 : 0);
    data.putByte(TAG_EXTENDING, mExtending ? 1 : 0);
    data.putInt(TAG_FACING, mFacing);
    data.put(TAG_ATTACHED_BLOCKS, attachedBlocksTag(mAttachedBlocks));
    return data;
}

void PistonArmBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    mState = data.getByte(TAG_STATE);
    mNewState = data.getByte(TAG_NEW_STATE);
    mProgress = data.getFloat(TAG_PROGRESS, 1.0f);
    mLastProgress = data.getFloat(TAG_LAST_PROGRESS, 1.0f);
    mSticky = data.getByte(TAG_STICKY) != 0;
    mExtending = data.getByte(TAG_EXTENDING) != 0;
    mFacing = data.getInt(TAG_FACING, 1);
    mFinished = mState != STATE_EXTENDING && mState != STATE_RETRACTING;

    mAttachedBlocks.clear();
    const Tag *blocks = data.get(TAG_ATTACHED_BLOCKS);
    if (blocks == nullptr || blocks->getType() != Tag::Type::List)
        return;

    const std::vector<Tag> &values = blocks->getList();
    for (size_t index = 0; index + 2 < values.size(); index += 3) {
        mAttachedBlocks.push_back(Vector3i(values[index].asInt(), values[index + 1].asInt(),
                                           values[index + 2].asInt()));
    }
}

Tag PistonArmBlockActor::getSpawnCompound() const {
    Tag data = Tag::ofCompound();
    data.putString("id", BLOCK_ACTOR_ID);
    data.putInt("x", mPosition.x);
    data.putInt("y", mPosition.y);
    data.putInt("z", mPosition.z);
    data.putByte(TAG_IS_MOVABLE, mFinished ? 1 : 0);
    data.putFloat(TAG_PROGRESS, mProgress);
    data.putFloat(TAG_LAST_PROGRESS, mLastProgress);
    data.put(TAG_ATTACHED_BLOCKS, attachedBlocksTag(mAttachedBlocks));
    data.put(TAG_BREAK_BLOCKS, Tag::ofList(Tag::Type::Int, {}));
    data.putByte(TAG_STICKY, mSticky ? 1 : 0);
    data.putByte(TAG_STATE, mState);
    data.putByte(TAG_NEW_STATE, mNewState);
    return data;
}

void PistonArmBlockActor::beginMove(bool extending, std::vector<Vector3i> attachedBlocks) {
    mFinished = false;
    mExtending = extending;
    mProgress = extending ? 0.0f : 1.0f;
    mLastProgress = mProgress;
    mState = extending ? STATE_EXTENDING : STATE_RETRACTING;
    mNewState = mState;
    mAttachedBlocks = std::move(attachedBlocks);
}

void PistonArmBlockActor::advance() {
    mLastProgress = mProgress;

    if (mExtending)
        mProgress = std::min(1.0f, mProgress + MOVE_STEP);
    else
        mProgress = std::max(0.0f, mProgress - MOVE_STEP);
}

void PistonArmBlockActor::finish() {
    mState = mExtending ? STATE_EXTENDED : STATE_RETRACTED;
    mNewState = mState;
    mAttachedBlocks.clear();
    mFinished = true;
}
