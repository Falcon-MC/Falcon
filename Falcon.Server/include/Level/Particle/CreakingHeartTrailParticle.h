#pragma once

#include "Core/Math/Vector3i.h"
#include "Level/Particle/Particle.h"

class CreakingHeartTrailParticle : public Particle {
public:
    CreakingHeartTrailParticle(const Vector3f &creakingPosition, const Vector3i &heartPosition);

    std::unique_ptr<Packet> encode() const override;

private:
    Vector3i mHeartPosition;
};
