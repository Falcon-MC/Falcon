#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"

#include <cstdint>
#include <string>

enum class CommandBlockActorMode : int32_t {
    Normal = 0,
    Repeating = 1,
    Chain = 2
};

class CommandBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "CommandBlock";

    static const int32_t CURRENT_VERSION = 10;

    static const char *const TAG_ID;
    static const char *const TAG_CONDITIONAL_MODE;
    static const char *const TAG_AUTO;
    static const char *const TAG_POWERED;
    static const char *const TAG_CUSTOM_NAME;
    static const char *const TAG_COMMAND;
    static const char *const TAG_LAST_EXECUTION;
    static const char *const TAG_TRACK_OUTPUT;
    static const char *const TAG_LAST_OUTPUT;
    static const char *const TAG_LAST_OUTPUT_PARAMS;
    static const char *const TAG_LP_COMMAND_MODE;
    static const char *const TAG_LP_CONDIONAL_MODE;
    static const char *const TAG_LP_REDSTONE_MODE;
    static const char *const TAG_SUCCESS_COUNT;
    static const char *const TAG_CONDITION_MET;
    static const char *const TAG_VERSION;
    static const char *const TAG_TICK_DELAY;
    static const char *const TAG_EXECUTE_ON_FIRST_TICK;
    static const char *const TAG_IS_MOVABLE;

    CommandBlockActor() = default;

    static CommandBlockActorMode modeFromBlockName(const std::string &name);

    CommandBlockActorMode getMode() const noexcept
    {
        return mMode;
    }

    void setMode(CommandBlockActorMode mode) noexcept
    {
        mMode = mode;
    }

    const std::string &getCommand() const noexcept
    {
        return mCommand;
    }

    void setCommand(const std::string &command)
    {
        mCommand = command;
        mSuccessCount = 0;
    }

    bool isAuto() const noexcept
    {
        return mAuto;
    }

    void setAuto(bool value) noexcept
    {
        mAuto = value;
    }

    bool needsRedstone() const noexcept
    {
        return !mAuto;
    }

    bool isConditional() const noexcept
    {
        return mConditionalMode;
    }

    void setConditional(bool value) noexcept
    {
        mConditionalMode = value;
    }

    bool isPowered() const noexcept
    {
        return mPowered;
    }

    void setPowered(bool value) noexcept
    {
        mPowered = value;
    }

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &tag, const PacketCodecContext &context) override;

    Tag getSpawnCompound() const override;

    CommandBlockActorMode mMode = CommandBlockActorMode::Normal;
    std::string mCommand;
    std::string mCustomName;
    std::string mLastOutput;
    bool mConditionalMode = false;
    bool mAuto = false;
    bool mPowered = false;
    bool mTrackOutput = true;
    bool mConditionMet = false;
    bool mExecutingOnFirstTick = false;
    int32_t mTickDelay = 0;
    int32_t mSuccessCount = 0;
    int32_t mLastOutputCommandMode = 0;
    bool mLastOutputCondionalMode = true;
    bool mLastOutputRedstoneMode = true;
    int64_t mLastExecution = 0;
    int32_t mCurrentTick = 0;
    uint64_t mRevision = 1;
};
