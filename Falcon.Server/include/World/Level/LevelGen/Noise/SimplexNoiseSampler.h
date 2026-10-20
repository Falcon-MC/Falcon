#pragma once

#include "World/Level/LevelGen/Noise/Noise.h"

class SimplexNoiseSampler : public Noise {
public:
    explicit SimplexNoiseSampler(IRandom &rand);

    double sample2D(double x, double y) const;

    double sample3D(double x, double y, double z) const;

private:
    double _cornerNoise3d(int32_t hash, double x, double y, double z, double max) const;
};
