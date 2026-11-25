#include "World/Level/Particle/ShriekParticle.h"

#include "Protocol/Packets/LevelEventGenericPacket.h"

namespace {
    const int32_t PARTICLE_SHRIEK = 2035;
}

ShriekParticle::ShriekParticle(const Vector3i &origin)
        : Particle(Vector3f((float) origin.x + 0.5f, (float) origin.y + 0.5f, (float) origin.z + 0.5f)),
          mOrigin(origin) {}

std::unique_ptr<Packet> ShriekParticle::encode() const {
    std::unique_ptr<LevelEventGenericPacket> packet(new LevelEventGenericPacket());
    packet->mEventId = PARTICLE_SHRIEK;
    packet->mData.putInt("originX", mOrigin.x);
    packet->mData.putInt("originY", mOrigin.y);
    packet->mData.putInt("originZ", mOrigin.z);
    return packet;
}
