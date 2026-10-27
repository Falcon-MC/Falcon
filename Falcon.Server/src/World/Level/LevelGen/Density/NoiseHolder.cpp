#include "World/Level/LevelGen/Density/NoiseHolder.h"

NoiseHolder::NoiseHolder() : mNoise(nullptr) {}

NoiseHolder::NoiseHolder(NormalNoisePtr noise) : mNoise(std::move(noise)) {}

double NoiseHolder::getValue(double x, double y, double z) const {
    if (mNoise == nullptr) {
        return 0.0;
    }

    float sampled = mNoise->getValue(x, y, z);
    return static_cast<double>(sampled);
}

double NoiseHolder::maxValue() const {
    if (mNoise == nullptr) {
        return 2.0;
    }

    return mNoise->getMax();
}
