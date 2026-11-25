#include "Protocol/Types/GatheringsConfigurationJoinInfo.h"

void GatheringsConfigurationJoinInfo::write(BinaryStream &stream) const {
    stream.putUuid(mExperienceId);
    stream.putString(mExperienceName);

    stream.putOptionalPresent(mHasWorldId);
    if (mHasWorldId)
        stream.putUuid(mWorldId);

    stream.putOptionalPresent(mHasWorldName);
    if (mHasWorldName)
        stream.putString(mWorldName);

    stream.putString(mCreatorId);

    stream.putOptionalPresent(mHasTargetId);
    if (mHasTargetId)
        stream.putUuid(mTargetId);

    stream.putOptionalPresent(mHasScenarioId);
    if (mHasScenarioId)
        stream.putString(mScenarioId);

    stream.putOptionalPresent(mHasServerId);
    if (mHasServerId)
        stream.putString(mServerId);
}

void GatheringsConfigurationJoinInfo::read(ReadOnlyBinaryStream &stream) {
    mExperienceId = stream.getUuid();
    mExperienceName = stream.getString();

    mHasWorldId = stream.getOptionalPresent();
    if (mHasWorldId)
        mWorldId = stream.getUuid();

    mHasWorldName = stream.getOptionalPresent();
    if (mHasWorldName)
        mWorldName = stream.getString();

    mCreatorId = stream.getString();

    mHasTargetId = stream.getOptionalPresent();
    if (mHasTargetId)
        mTargetId = stream.getUuid();

    mHasScenarioId = stream.getOptionalPresent();
    if (mHasScenarioId)
        mScenarioId = stream.getString();

    mHasServerId = stream.getOptionalPresent();
    if (mHasServerId)
        mServerId = stream.getString();
}
