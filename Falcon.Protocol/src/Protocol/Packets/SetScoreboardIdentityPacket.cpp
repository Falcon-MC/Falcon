#include "Protocol/Packets/SetScoreboardIdentityPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetScoreboardIdentityPacket::SetScoreboardIdentityPacket() = default;

void SetScoreboardIdentityPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mAction);
    stream.putArrayLength((uint32_t) mEntries.size());
    for (const ScoreboardIdentityEntry &entry: mEntries) {
        stream.putVarLong(entry.mScoreboardId);

        stream.putBool(mAction == Action::Add);
        if (mAction == Action::Add)
            stream.putVarLong(entry.mPlayerId);
    }
}

void SetScoreboardIdentityPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAction = (Action) stream.getByte();
    mEntries.clear();
    uint32_t count = stream.getArrayLength();
    for (uint32_t i = 0; i < count; i++) {
        ScoreboardIdentityEntry entry;
        entry.mScoreboardId = stream.getVarLong();

        if (stream.getBool())
            entry.mPlayerId = stream.getVarLong();

        mEntries.push_back(entry);
    }
}

void SetScoreboardIdentityPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
