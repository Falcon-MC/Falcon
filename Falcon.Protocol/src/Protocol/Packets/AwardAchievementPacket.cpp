#include "Protocol/Packets/AwardAchievementPacket.h"

#include "Protocol/NetworkPacketHandler.h"

AwardAchievementPacket::AwardAchievementPacket()
        : mAchievementId(0) {}

void AwardAchievementPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLInt((uint32_t) mAchievementId);
}

void AwardAchievementPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAchievementId = stream.getSignedLInt();
}

void AwardAchievementPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
