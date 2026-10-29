#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <array>
#include <cstdint>

class NoiseF {
public:
    virtual ~NoiseF() = default;

    static int32_t floor(float x);

    static float fade(float x);

    static float lerp(float x, float y, float z);

    static float linearLerp(float x, float x1, float x2, float q0, float q1);

    static float grad(int32_t hash, float x, float y, float z);

    virtual float getNoise2D(float x, float z) const = 0;

    virtual float getNoise3D(float x, float y, float z) const = 0;

    float noise2D(float x, float z) const;

    float noise2D(float x, float z, bool normalized) const;

    float noise3D(float x, float y, float z) const;

    float noise3D(float x, float y, float z, bool normalized) const;

    void setOffset(float x, float y, float z);

protected:
    NoiseF();

    std::array<int32_t, 512> mPerm;
    float mOffsetX;
    float mOffsetY;
    float mOffsetZ;
    float mOctaves;
    float mPersistence;
    float mExpansion;
};

class PerlinF : public NoiseF {
public:
    PerlinF(IRandom &random, float octaves, float persistence);

    PerlinF(IRandom &random, float octaves, float persistence, float expansion);

    float getValue(float x, float y, float z) const;

    float getNoise2D(float x, float y) const override;

    float getNoise3D(float x, float y, float z) const override;
};

class SimplexF : public PerlinF {
public:
    SimplexF(IRandom &random, float octaves, float persistence);

    SimplexF(IRandom &random, float octaves, float persistence, float expansion);

    float getNoise2D(float x, float y) const override;

    float getNoise3D(float x, float y, float z) const override;

protected:
    static float dot2D(const int32_t *g, float x, float y);

    static float dot3D(const int32_t *g, float x, float y, float z);

    float mOffsetW;
};
