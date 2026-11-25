#include "World/Level/Particle/SonicExplosionParticle.h"

#include "Protocol/Packets/LevelEventGenericPacket.h"

namespace {
    const int32_t PARTICLE_SONIC_EXPLOSION = 2039;
}

std::unique_ptr<Packet> SonicExplosionParticle::encode() const {
    std::unique_ptr<LevelEventGenericPacket> packet(new LevelEventGenericPacket());
    packet->mEventId = PARTICLE_SONIC_EXPLOSION;
    packet->mData.putFloat("x", mPosition.x);
    packet->mData.putFloat("y", mPosition.y);
    packet->mData.putFloat("z", mPosition.z);
    return packet;
}
