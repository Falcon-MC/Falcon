#include "Protocol/Types/StartGameTypes.h"

void ServerConfigurationJoinInfo::write(BinaryStream &stream) const {
    stream.putOptionalPresent(mHasGatheringsConfiguration);
    if (mHasGatheringsConfiguration)
        mGatheringsConfiguration.write(stream);

    stream.putOptionalPresent(mHasClientStoreEntrypoint);
    if (mHasClientStoreEntrypoint) {
        stream.putString(mClientStoreEntrypoint.mStoreId);
        stream.putString(mClientStoreEntrypoint.mStoreName);
    }

    stream.putOptionalPresent(mHasPresenceConfiguration);
    if (mHasPresenceConfiguration) {
        stream.putOptionalPresent(mPresenceConfiguration.mHasRichPresenceId);
        if (mPresenceConfiguration.mHasRichPresenceId)
            stream.putString(mPresenceConfiguration.mRichPresenceId);
    }
}

void ServerConfigurationJoinInfo::read(ReadOnlyBinaryStream &stream) {
    mHasGatheringsConfiguration = stream.getOptionalPresent();
    if (mHasGatheringsConfiguration)
        mGatheringsConfiguration.read(stream);

    mHasClientStoreEntrypoint = stream.getOptionalPresent();
    if (mHasClientStoreEntrypoint) {
        mClientStoreEntrypoint.mStoreId = stream.getString();
        mClientStoreEntrypoint.mStoreName = stream.getString();
    }

    mHasPresenceConfiguration = stream.getOptionalPresent();
    if (mHasPresenceConfiguration) {
        mPresenceConfiguration.mHasRichPresenceId = stream.getOptionalPresent();
        if (mPresenceConfiguration.mHasRichPresenceId)
            mPresenceConfiguration.mRichPresenceId = stream.getString();
    }
}

GameRuleData GameRuleData::ofBool(const std::string &name, bool value, bool editable) {
    GameRuleData rule;
    rule.mName = name;
    rule.mEditable = editable;
    rule.mType = Type::Bool;
    rule.mBoolValue = value;
    return rule;
}

GameRuleData GameRuleData::ofInt(const std::string &name, int32_t value, bool editable) {
    GameRuleData rule;
    rule.mName = name;
    rule.mEditable = editable;
    rule.mType = Type::Int;
    rule.mIntValue = value;
    return rule;
}

GameRuleData GameRuleData::ofFloat(const std::string &name, float value, bool editable) {
    GameRuleData rule;
    rule.mName = name;
    rule.mEditable = editable;
    rule.mType = Type::Float;
    rule.mFloatValue = value;
    return rule;
}

void GameRuleData::write(BinaryStream &stream) const {
    stream.putString(mName);
    stream.putBool(mEditable);
    stream.putUnsignedVarInt((uint32_t) mType);

    switch (mType) {
        case Type::Bool:
            stream.putBool(mBoolValue);
            break;
        case Type::Int:
            stream.putLInt((uint32_t) mIntValue);
            break;
        case Type::Float:
            stream.putLFloat(mFloatValue);
            break;
        default:
            throw BinaryDataException("Unknown game rule type");
    }
}

void GameRuleData::read(ReadOnlyBinaryStream &stream) {
    mName = stream.getString();
    mEditable = stream.getBool();
    mType = (Type) stream.getUnsignedVarInt();

    switch (mType) {
        case Type::Bool:
            mBoolValue = stream.getBool();
            break;
        case Type::Int:
            mIntValue = stream.getSignedLInt();
            break;
        case Type::Float:
            mFloatValue = stream.getLFloat();
            break;
        default:
            throw BinaryDataException("Unknown game rule type");
    }
}

void writeExperiments(BinaryStream &stream, const std::vector<ExperimentData> &experiments) {
    // the experiment count is a plain little endian int, not a varint
    stream.putLInt((uint32_t) experiments.size());

    for (const ExperimentData &experiment: experiments) {
        stream.putString(experiment.mName);
        stream.putBool(experiment.mEnabled);
    }
}

void readExperiments(ReadOnlyBinaryStream &stream, std::vector<ExperimentData> &experiments) {
    const uint32_t count = stream.getLInt();
    if (count > stream.getEncodingSettings().mMaxListSize)
        throw BinaryDataException("Tried to read " + std::to_string(count) + " experiments");

    experiments.clear();
    experiments.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        ExperimentData experiment;
        experiment.mName = stream.getString();
        experiment.mEnabled = stream.getBool();
        experiments.push_back(experiment);
    }
}
