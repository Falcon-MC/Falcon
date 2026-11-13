#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Particle/Particle.h"

// the signal a listener sends from the vibration's block to itself (sub_143FA9A60, 26.60)
class VibrationSignalParticle : public Particle {
public:
    VibrationSignalParticle(const Vector3i &origin, const Vector3f &target, float timeToLive);

    std::unique_ptr<Packet> encode() const override;

private:
    Vector3f mTarget;
    float mTimeToLive;
};
