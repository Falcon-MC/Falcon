#include "Protocol/Packets/SetScorePacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    const char *getScorerTypeName(ScorerType type) {
        switch (type) {
            case ScorerType::Invalid:
                return "remove";
            case ScorerType::Player:
                return "changeplayer";
            case ScorerType::Entity:
                return "changeentity";
            case ScorerType::Fake:
                return "changefakeplayer";
        }

        return "remove";
    }

}

SetScorePacket::SetScorePacket() = default;

void SetScorePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mInfos.size());
    for (const ScoreInfoEntry &info: mInfos) {
        stream.putUnsignedVarInt((uint32_t) info.mType);
        stream.putString(getScorerTypeName(info.mType));
        stream.putVarLong(info.mScoreboardId);

        switch (info.mType) {
            case ScorerType::Invalid:
                stream.putByte(1);
                stream.putOptionalPresent(!info.mObjectiveId.empty());
                if (!info.mObjectiveId.empty())
                    stream.putString(info.mObjectiveId);
                break;
            case ScorerType::Player:
            case ScorerType::Entity:
                stream.putString(info.mObjectiveId);
                stream.putLInt((uint32_t) info.mScore);
                stream.putVarLong(info.mActorId);
                break;
            case ScorerType::Fake:
                stream.putString(info.mObjectiveId);
                stream.putLInt((uint32_t) info.mScore);
                stream.putString(info.mName.empty() ? " " : info.mName);
                break;
        }
    }
}

void SetScorePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mInfos.clear();
    uint32_t count = stream.getArrayLength();
    for (uint32_t i = 0; i < count; i++) {
        ScoreInfoEntry info;
        info.mType = (ScorerType) stream.getUnsignedVarInt();
        stream.getString();
        info.mScoreboardId = stream.getVarLong();

        switch (info.mType) {
            case ScorerType::Invalid:
                stream.getByte();
                if (stream.getOptionalPresent())
                    info.mObjectiveId = stream.getString();
                break;
            case ScorerType::Player:
            case ScorerType::Entity:
                info.mObjectiveId = stream.getString();
                info.mScore = (int32_t) stream.getLInt();
                info.mActorId = stream.getVarLong();
                break;
            case ScorerType::Fake:
                info.mObjectiveId = stream.getString();
                info.mScore = (int32_t) stream.getLInt();
                info.mName = stream.getString();
                break;
        }

        mInfos.push_back(info);
    }
}

void SetScorePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
