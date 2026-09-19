#include "Level/Particle/ExplodeParticle.h"

#include "Protocol/Packets/LevelEventPacket.h"

#include <cmath>

ExplodeParticle::ExplodeParticle(const Vector3f &position, double size)
        : Particle(position), mSize(size) {
}

std::unique_ptr<Packet> ExplodeParticle::encode() const {
    std::unique_ptr<LevelEventPacket> packet(new LevelEventPacket());
    packet->mEventId = LevelEventPacket::Event::ParticleExplode;
    packet->mPosition = mPosition;
    packet->mData = (int32_t) std::lround((float) mSize);
    return packet;
}
