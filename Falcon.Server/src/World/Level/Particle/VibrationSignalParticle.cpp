#include "World/Level/Particle/VibrationSignalParticle.h"

#include "Protocol/Packets/LevelEventGenericPacket.h"

namespace {
    const int32_t PARTICLE_VIBRATION_SIGNAL = 2027;
    const float SIGNAL_SPEED = 20.0f;

    // a vec3 PositionSource as sub_143FA7470 writes it
    Tag positionSource(const Vector3f &position) {
        Tag source = Tag::ofCompound();
        source.putString("type", "vec3");
        source.putFloat("x", position.x);
        source.putFloat("y", position.y);
        source.putFloat("z", position.z);
        return source;
    }
}

VibrationSignalParticle::VibrationSignalParticle(const Vector3i &origin, const Vector3f &target, float timeToLive)
        : Particle(Vector3f((float) origin.x + 0.5f, (float) origin.y + 0.5f, (float) origin.z + 0.5f)),
          mTarget(target), mTimeToLive(timeToLive) {}

std::unique_ptr<Packet> VibrationSignalParticle::encode() const {
    std::unique_ptr<LevelEventGenericPacket> packet(new LevelEventGenericPacket());
    packet->mEventId = PARTICLE_VIBRATION_SIGNAL;
    packet->mData.put("origin", positionSource(mPosition));
    packet->mData.put("target", positionSource(mTarget));
    packet->mData.putFloat("speed", SIGNAL_SPEED);
    packet->mData.putFloat("timeToLive", mTimeToLive);
    return packet;
}
