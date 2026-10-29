#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"

class NoiseHolder {
public:
    NoiseHolder();

    explicit NoiseHolder(NormalNoisePtr noise);

    double getValue(double x, double y, double z) const;

    double maxValue() const;

private:
    NormalNoisePtr mNoise;
};
