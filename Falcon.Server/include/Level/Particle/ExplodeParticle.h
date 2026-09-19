#pragma once

#include "Level/Particle/Particle.h"

class ExplodeParticle : public Particle {
public:
    ExplodeParticle(const Vector3f &position, double size);

    std::unique_ptr<Packet> encode() const override;

private:
    double mSize;
};
