#include "Protocol/Packets/LegacyTelemetryEventPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    int32_t toPayloadType(EventDataType type) {
        switch (type) {
            case EventDataType::AchievementAwarded:
                return 0;
            case EventDataType::EntityInteract:
                return 1;
            case EventDataType::PortalBuilt:
                return 2;
            case EventDataType::PortalUsed:
                return 3;
            case EventDataType::MobKilled:
                return 4;
            case EventDataType::CauldronUsed:
                return 5;
            case EventDataType::PlayerDied:
                return 6;
            case EventDataType::BossKilled:
                return 7;
            case EventDataType::SlashCommandExecuted:
                return 8;
            default:
                return 0;
        }
    }

    std::string joinBySemicolon(const std::vector<std::string> &values) {
        std::string result;
        for (size_t i = 0; i < values.size(); i++) {
            if (i > 0)
                result += ";";
            result += values[i];
        }
        return result;
    }

    std::vector<std::string> splitBySemicolon(const std::string &value) {
        std::vector<std::string> result;
        size_t start = 0;
        while (true) {
            size_t separator = value.find(';', start);
            if (separator == std::string::npos) {
                result.push_back(value.substr(start));
                break;
            }
            result.push_back(value.substr(start, separator - start));
            start = separator + 1;
        }
        return result;
    }

}

LegacyTelemetryEventPacket::LegacyTelemetryEventPacket() = default;

void LegacyTelemetryEventPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mUniqueActorId);
    stream.putVarInt((int32_t) mEventData.mType);
    stream.putBool(mUsePlayerId);
    stream.putUnsignedVarInt((uint32_t) toPayloadType(mEventData.mType));

    switch (mEventData.mType) {
        case EventDataType::AchievementAwarded:
            stream.putVarInt(mEventData.mAchievementId);
            break;
        case EventDataType::EntityInteract:
            stream.putVarInt(mEventData.mInteractionType);
            stream.putVarInt(mEventData.mLegacyActorTypeId);
            stream.putVarInt(mEventData.mActorVariant);
            stream.putByte((unsigned char) mEventData.mPaletteColor);
            break;
        case EventDataType::PortalBuilt:
            stream.putVarInt(mEventData.mDimensionId);
            break;
        case EventDataType::PortalUsed:
            stream.putVarInt(mEventData.mFromDimensionId);
            stream.putVarInt(mEventData.mToDimensionId);
            break;
        case EventDataType::MobKilled:
            stream.putVarLong(mEventData.mKillerUniqueActorId);
            stream.putVarLong(mEventData.mVictimUniqueActorId);
            stream.putVarInt(mEventData.mActorDamageCause);
            stream.putVarInt(mEventData.mVillagerTradeTier);
            stream.putString(mEventData.mVillagerDisplayName);
            break;
        case EventDataType::CauldronUsed:
            stream.putUnsignedVarInt((uint32_t) mEventData.mPotionId);
            stream.putVarInt(mEventData.mColor);
            stream.putVarInt(mEventData.mFillLevel);
            break;
        case EventDataType::PlayerDied:
            stream.putVarInt(mEventData.mAttackerActorId);
            stream.putVarInt(mEventData.mActorDamageCause);
            break;
        case EventDataType::BossKilled:
            stream.putVarLong(mEventData.mBossUniqueActorId);
            stream.putVarInt(mEventData.mPlayerPartySize);
            stream.putVarInt(mEventData.mBossActorType);
            break;
        case EventDataType::AgentCommand:
            stream.putVarInt((int32_t) mEventData.mAgentResult);
            stream.putVarInt(mEventData.mAgentDataValue);
            stream.putString(mEventData.mAgentCommand);
            stream.putString(mEventData.mAgentDataKey);
            stream.putString(mEventData.mAgentOutput);
            break;
        case EventDataType::AgentCreated:
            break;
        case EventDataType::PatternRemoved:
            stream.putVarInt(mEventData.mPatternItemId);
            stream.putVarInt(mEventData.mPatternAuxValue);
            stream.putVarInt(mEventData.mPatternsSize);
            stream.putVarInt(mEventData.mPatternIndex);
            stream.putVarInt(mEventData.mPatternColor);
            break;
        case EventDataType::SlashCommandExecuted:
            stream.putVarInt(mEventData.mSlashSuccessCount);
            stream.putVarInt(mEventData.mSlashErrorCount);
            stream.putString(mEventData.mSlashCommandName);
            stream.putString(joinBySemicolon(mEventData.mSlashOutputMessages));
            break;
        case EventDataType::FishBucketed:
            stream.putVarInt(mEventData.mFishPattern);
            stream.putVarInt(mEventData.mFishPreset);
            stream.putVarInt(mEventData.mFishBucketedActorType);
            stream.putBool(mEventData.mFishReleaseEvent);
            break;
        default:
            break;
    }
}

void LegacyTelemetryEventPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueActorId = stream.getVarLong();
    mEventData = EventData();
    mEventData.mType = (EventDataType) stream.getVarInt();
    mUsePlayerId = stream.getBool();
    stream.getUnsignedVarInt();

    switch (mEventData.mType) {
        case EventDataType::AchievementAwarded:
            mEventData.mAchievementId = stream.getVarInt();
            break;
        case EventDataType::EntityInteract:
            mEventData.mInteractionType = stream.getVarInt();
            mEventData.mLegacyActorTypeId = stream.getVarInt();
            mEventData.mActorVariant = stream.getVarInt();
            mEventData.mPaletteColor = stream.getByte();
            break;
        case EventDataType::PortalBuilt:
            mEventData.mDimensionId = stream.getVarInt();
            break;
        case EventDataType::PortalUsed:
            mEventData.mFromDimensionId = stream.getVarInt();
            mEventData.mToDimensionId = stream.getVarInt();
            break;
        case EventDataType::MobKilled:
            mEventData.mKillerUniqueActorId = stream.getVarLong();
            mEventData.mVictimUniqueActorId = stream.getVarLong();
            mEventData.mActorDamageCause = stream.getVarInt();
            mEventData.mVillagerTradeTier = stream.getVarInt();
            mEventData.mVillagerDisplayName = stream.getString();
            break;
        case EventDataType::CauldronUsed:
            mEventData.mPotionId = stream.getVarInt();
            mEventData.mColor = stream.getVarInt();
            mEventData.mFillLevel = stream.getVarInt();
            break;
        case EventDataType::PlayerDied:
            mEventData.mAttackerActorId = stream.getVarInt();
            mEventData.mActorDamageCause = stream.getVarInt();
            break;
        case EventDataType::BossKilled:
            mEventData.mBossUniqueActorId = stream.getVarLong();
            mEventData.mPlayerPartySize = stream.getVarInt();
            mEventData.mBossActorType = stream.getVarInt();
            break;
        case EventDataType::AgentCommand:
            mEventData.mAgentResult = (AgentResult) stream.getVarInt();
            mEventData.mAgentDataValue = stream.getVarInt();
            mEventData.mAgentCommand = stream.getString();
            mEventData.mAgentDataKey = stream.getString();
            mEventData.mAgentOutput = stream.getString();
            break;
        case EventDataType::AgentCreated:
            break;
        case EventDataType::PatternRemoved:
            mEventData.mPatternItemId = stream.getVarInt();
            mEventData.mPatternAuxValue = stream.getVarInt();
            mEventData.mPatternsSize = stream.getVarInt();
            mEventData.mPatternIndex = stream.getVarInt();
            mEventData.mPatternColor = stream.getVarInt();
            break;
        case EventDataType::SlashCommandExecuted:
            mEventData.mSlashSuccessCount = stream.getVarInt();
            mEventData.mSlashErrorCount = stream.getVarInt();
            mEventData.mSlashCommandName = stream.getString();
            mEventData.mSlashOutputMessages = splitBySemicolon(stream.getString());
            break;
        case EventDataType::FishBucketed:
            mEventData.mFishPattern = stream.getVarInt();
            mEventData.mFishPreset = stream.getVarInt();
            mEventData.mFishBucketedActorType = stream.getVarInt();
            mEventData.mFishReleaseEvent = stream.getBool();
            break;
        default:
            break;
    }
}

void LegacyTelemetryEventPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
