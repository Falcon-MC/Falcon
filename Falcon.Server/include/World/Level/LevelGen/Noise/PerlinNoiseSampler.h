#pragma once

#include "World/Level/LevelGen/Noise/Noise.h"

class PerlinNoiseSampler : public Noise {
public:
    explicit PerlinNoiseSampler(IRandom &rand);

    double sample(double x, double y, double z, double yAmplification, double minY) const;

    // already-scaled coordinates
    float sampleFloat(float x, float y, float z) const;

private:
    double _sample(int32_t sectionX, int32_t sectionY, int32_t sectionZ, double localX, double localY, double localZ, double fadeLocalY) const;

    float _sampleFloat(int32_t sectionX, int32_t sectionY, int32_t sectionZ, float localX, float localY, float localZ) const;

    float mOriginXFloat;
    float mOriginYFloat;
    float mOriginZFloat;
};
