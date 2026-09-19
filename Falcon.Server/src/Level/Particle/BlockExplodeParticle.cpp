#include "Level/Particle/BlockExplodeParticle.h"

#include "Protocol/Packets/LevelEventGenericPacket.h"

#include <string>

BlockExplodeParticle::BlockExplodeParticle(const Vector3f &position, double radius,
                                           const std::vector<Vector3i> &blocks)
        : Particle(position), mRadius(radius), mBlocks(blocks) {
}

std::unique_ptr<Packet> BlockExplodeParticle::encode() const {
    std::unique_ptr<LevelEventGenericPacket> packet(new LevelEventGenericPacket());
    packet->mEventId = LevelEventGenericPacket::Event::ParticleBlockExplode;
    packet->mData.putFloat("originX", mPosition.x);
    packet->mData.putFloat("originY", mPosition.y);
    packet->mData.putFloat("originZ", mPosition.z);
    packet->mData.putFloat("radius", (float) mRadius);
    packet->mData.putInt("size", (int32_t) mBlocks.size());

    for (size_t i = 0; i < mBlocks.size(); ++i) {
        const std::string prefix = "pos" + std::to_string(i);
        packet->mData.putFloat(prefix + "x", (float) mBlocks[i].x);
        packet->mData.putFloat(prefix + "y", (float) mBlocks[i].y);
        packet->mData.putFloat(prefix + "z", (float) mBlocks[i].z);
    }

    return packet;
}
