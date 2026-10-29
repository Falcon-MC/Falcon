#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <memory>

class Particle {
public:
    explicit Particle(const Vector3f &position)
            : mPosition(position) {
    }

    virtual ~Particle() = default;

    const Vector3f &getPosition() const {
        return mPosition;
    }

    virtual std::unique_ptr<Packet> encode() const = 0;

protected:
    Vector3f mPosition;
};
