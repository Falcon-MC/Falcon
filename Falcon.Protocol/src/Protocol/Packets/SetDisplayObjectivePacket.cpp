#include "Protocol/Packets/SetDisplayObjectivePacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetDisplayObjectivePacket::SetDisplayObjectivePacket() = default;

void SetDisplayObjectivePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mDisplaySlot);
    stream.putString(mObjectiveId);
    stream.putString(mDisplayName);
    stream.putString(mCriteria);
    stream.putVarInt(mSortOrder);
}

void SetDisplayObjectivePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mDisplaySlot = stream.getString();
    mObjectiveId = stream.getString();
    mDisplayName = stream.getString();
    mCriteria = stream.getString();
    mSortOrder = stream.getVarInt();
}

void SetDisplayObjectivePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
