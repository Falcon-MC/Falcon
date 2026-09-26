#include "Level/Particle/CreakingHeartTrailParticle.h"

#include "Protocol/Packets/LevelEventGenericPacket.h"

namespace {
    const int32_t PARTICLE_CREAKING_HEART_TRAIL = 9816;
    const int32_t TRAIL_AMOUNT = 1;
}

CreakingHeartTrailParticle::CreakingHeartTrailParticle(const Vector3f &creakingPosition,
                                                       const Vector3i &heartPosition)
        : Particle(creakingPosition), mHeartPosition(heartPosition) {
}

std::unique_ptr<Packet> CreakingHeartTrailParticle::encode() const {
    std::unique_ptr<LevelEventGenericPacket> packet(new LevelEventGenericPacket());
    packet->mEventId = PARTICLE_CREAKING_HEART_TRAIL;
    packet->mData.putInt("CreakingAmount", TRAIL_AMOUNT);
    packet->mData.putFloat("CreakingX", mPosition.x);
    packet->mData.putFloat("CreakingY", mPosition.y);
    packet->mData.putFloat("CreakingZ", mPosition.z);
    packet->mData.putInt("HeartAmount", TRAIL_AMOUNT);
    packet->mData.putFloat("HeartX", (float) mHeartPosition.x);
    packet->mData.putFloat("HeartY", (float) mHeartPosition.y);
    packet->mData.putFloat("HeartZ", (float) mHeartPosition.z);
    return packet;
}
