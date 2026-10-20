#include "World/Level/Block/Actor/CommandBlockActor.h"

const char *const CommandBlockActor::TAG_ID = "id";
const char *const CommandBlockActor::TAG_CONDITIONAL_MODE = "conditionalMode";
const char *const CommandBlockActor::TAG_AUTO = "auto";
const char *const CommandBlockActor::TAG_POWERED = "powered";
const char *const CommandBlockActor::TAG_CUSTOM_NAME = "CustomName";
const char *const CommandBlockActor::TAG_COMMAND = "Command";
const char *const CommandBlockActor::TAG_LAST_EXECUTION = "LastExecution";
const char *const CommandBlockActor::TAG_TRACK_OUTPUT = "TrackOutput";
const char *const CommandBlockActor::TAG_LAST_OUTPUT = "LastOutput";
const char *const CommandBlockActor::TAG_LAST_OUTPUT_PARAMS = "LastOutputParams";
const char *const CommandBlockActor::TAG_LP_COMMAND_MODE = "LPCommandMode";
const char *const CommandBlockActor::TAG_LP_CONDIONAL_MODE = "LPCondionalMode";
const char *const CommandBlockActor::TAG_LP_REDSTONE_MODE = "LPRedstoneMode";
const char *const CommandBlockActor::TAG_SUCCESS_COUNT = "SuccessCount";
const char *const CommandBlockActor::TAG_CONDITION_MET = "conditionMet";
const char *const CommandBlockActor::TAG_VERSION = "Version";
const char *const CommandBlockActor::TAG_TICK_DELAY = "TickDelay";
const char *const CommandBlockActor::TAG_EXECUTE_ON_FIRST_TICK = "ExecuteOnFirstTick";
const char *const CommandBlockActor::TAG_IS_MOVABLE = "isMovable";

CommandBlockActorMode CommandBlockActor::modeFromBlockName(const std::string &name)
{
    if (name == "minecraft:repeating_command_block")
        return CommandBlockActorMode::Repeating;

    if (name == "minecraft:chain_command_block")
        return CommandBlockActorMode::Chain;

    return CommandBlockActorMode::Normal;
}

Tag CommandBlockActor::saveNbt() const
{
    Tag tag = Tag::ofCompound();
    tag.putString(TAG_ID, "CommandBlock");
    tag.putInt("x", mPosition.x);
    tag.putInt("y", mPosition.y);
    tag.putInt("z", mPosition.z);
    tag.putBool(TAG_IS_MOVABLE, true);
    tag.putBool(TAG_POWERED, mPowered);
    tag.putBool(TAG_CONDITIONAL_MODE, mConditionalMode);
    tag.putBool(TAG_AUTO, mAuto);
    tag.putString(TAG_COMMAND, mCommand);
    tag.putLong(TAG_LAST_EXECUTION, mLastExecution);
    tag.putBool(TAG_TRACK_OUTPUT, mTrackOutput);
    tag.putString(TAG_LAST_OUTPUT, mLastOutput);
    tag.put(TAG_LAST_OUTPUT_PARAMS, Tag::ofList(Tag::Type::String));
    tag.putInt(TAG_LP_COMMAND_MODE, mLastOutputCommandMode);
    tag.putBool(TAG_LP_CONDIONAL_MODE, mLastOutputCondionalMode);
    tag.putBool(TAG_LP_REDSTONE_MODE, mLastOutputRedstoneMode);
    tag.putInt(TAG_SUCCESS_COUNT, mSuccessCount);
    tag.putBool(TAG_CONDITION_MET, mConditionMet);
    tag.putInt(TAG_VERSION, CURRENT_VERSION);
    tag.putInt(TAG_TICK_DELAY, mTickDelay);
    tag.putBool(TAG_EXECUTE_ON_FIRST_TICK, mExecutingOnFirstTick);

    if (!mCustomName.empty())
        tag.putString(TAG_CUSTOM_NAME, mCustomName);

    return tag;
}

void CommandBlockActor::loadNbt(const Tag &tag, const PacketCodecContext &context)
{
    (void) context;

    if (!tag.isCompound())
        return;

    mPosition = Vector3i(tag.getInt("x", mPosition.x), tag.getInt("y", mPosition.y), tag.getInt("z", mPosition.z));
    mPowered = tag.getBool(TAG_POWERED, false);
    mConditionalMode = tag.getBool(TAG_CONDITIONAL_MODE, false);
    mAuto = tag.getBool(TAG_AUTO, false);
    mCommand = tag.getString(TAG_COMMAND, std::string());
    mLastExecution = tag.getLong(TAG_LAST_EXECUTION, 0);
    mTrackOutput = tag.getBool(TAG_TRACK_OUTPUT, true);
    mLastOutput = tag.getString(TAG_LAST_OUTPUT, std::string());
    mLastOutputCommandMode = tag.getInt(TAG_LP_COMMAND_MODE, 0);
    mLastOutputCondionalMode = tag.getBool(TAG_LP_CONDIONAL_MODE, true);
    mLastOutputRedstoneMode = tag.getBool(TAG_LP_REDSTONE_MODE, true);
    mSuccessCount = tag.getInt(TAG_SUCCESS_COUNT, 0);
    mConditionMet = tag.getBool(TAG_CONDITION_MET, false);
    mTickDelay = tag.getInt(TAG_TICK_DELAY, 0);
    mExecutingOnFirstTick = tag.getBool(TAG_EXECUTE_ON_FIRST_TICK, false);
    mCustomName = tag.getString(TAG_CUSTOM_NAME, std::string());
}

Tag CommandBlockActor::getSpawnCompound() const
{
    return saveNbt();
}
