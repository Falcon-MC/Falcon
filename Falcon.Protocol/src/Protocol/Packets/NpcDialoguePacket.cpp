#include "Protocol/Packets/NpcDialoguePacket.h"

#include "Protocol/NetworkPacketHandler.h"

NpcDialoguePacket::NpcDialoguePacket() = default;

void NpcDialoguePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLLong((uint64_t) mUniqueActorId);
    stream.putVarInt((int32_t) mAction);
    stream.putString(mDialogue);
    stream.putString(mSceneName);
    stream.putString(mNpcName);
    stream.putString(mActionJson);
}

void NpcDialoguePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueActorId = (int64_t) stream.getLLong();
    mAction = (Action) stream.getVarInt();
    mDialogue = stream.getString();
    mSceneName = stream.getString();
    mNpcName = stream.getString();
    mActionJson = stream.getString();
}

void NpcDialoguePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
