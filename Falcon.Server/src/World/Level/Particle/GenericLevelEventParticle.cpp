#include "World/Level/Particle/GenericLevelEventParticle.h"

#include "Protocol/Packets/LevelEventGenericPacket.h"

std::unique_ptr<Packet> GenericLevelEventParticle::encode() const {
    std::unique_ptr<LevelEventGenericPacket> packet(new LevelEventGenericPacket());
    packet->mEventId = mEventId;
    packet->mData = mData;
    return packet;
}
