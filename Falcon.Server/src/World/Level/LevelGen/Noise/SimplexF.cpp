#include "World/Level/LevelGen/Noise/SimplexF.h"

#include <cmath>

namespace {

const int32_t GRAD3[12][3] = {
    {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0},
    {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
    {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}
};

const float SQRT_3 = static_cast<float>(std::sqrt(3.0));
const float SQRT_5 = static_cast<float>(std::sqrt(5.0));
const float F2 = 0.5f * (SQRT_3 - 1.0f);
const float G2 = (3.0f - SQRT_3) / 6.0f;
const float G22 = G2 * 2.0f - 1.0f;
const float F3 = 1.0f / 3.0f;
const float G3 = 1.0f / 6.0f;
const float F4 = (SQRT_5 - 1.0f) / 4.0f;
const float G4 = (5.0f - SQRT_5) / 20.0f;
const float G42 = G4 * 2.0f;
const float G43 = G4 * 3.0f;
const float G44 = G4 * 4.0f - 1.0f;

}

NoiseF::NoiseF()
    : mPerm(), mOffsetX(0.0f), mOffsetY(0.0f), mOffsetZ(0.0f), mOctaves(8.0f), mPersistence(0.0f), mExpansion(0.0f) {
}

int32_t NoiseF::floor(float x) {
    return x >= 0.0f ? static_cast<int32_t>(x) : static_cast<int32_t>(x - 1.0f);
}

float NoiseF::fade(float x) {
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

float NoiseF::lerp(float x, float y, float z) {
    return y + x * (z - y);
}

float NoiseF::linearLerp(float x, float x1, float x2, float q0, float q1) {
    return ((x2 - x) / (x2 - x1)) * q0 + ((x - x1) / (x2 - x1)) * q1;
}

float NoiseF::grad(int32_t hash, float x, float y, float z) {
    hash &= 15;
    const float u = hash < 8 ? x : y;
    const float v = hash < 4 ? y : ((hash == 12 || hash == 14) ? x : z);

    return ((hash & 1) == 0 ? u : -u) + ((hash & 2) == 0 ? v : -v);
}

float NoiseF::noise2D(float x, float z) const {
    return noise2D(x, z, false);
}

float NoiseF::noise2D(float x, float z, bool normalized) const {
    float result = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float max = 0.0f;

    x *= mExpansion;
    z *= mExpansion;

    for (int32_t i = 0; static_cast<float>(i) < mOctaves; ++i) {
        result += getNoise2D(x * freq, z * freq) * amp;
        max += amp;
        freq *= 2.0f;
        amp *= mPersistence;
    }

    if (normalized) {
        result /= max;
    }

    return result;
}

float NoiseF::noise3D(float x, float y, float z) const {
    return noise3D(x, y, z, false);
}

float NoiseF::noise3D(float x, float y, float z, bool normalized) const {
    float result = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float max = 0.0f;

    x *= mExpansion;
    y *= mExpansion;
    z *= mExpansion;

    for (int32_t i = 0; static_cast<float>(i) < mOctaves; ++i) {
        result += getNoise3D(x * freq, y * freq, z * freq) * amp;
        max += amp;
        freq *= 2.0f;
        amp *= mPersistence;
    }

    if (normalized) {
        result /= max;
    }

    return result;
}

void NoiseF::setOffset(float x, float y, float z) {
    mOffsetX = x;
    mOffsetY = y;
    mOffsetZ = z;
}

PerlinF::PerlinF(IRandom &random, float octaves, float persistence)
    : PerlinF(random, octaves, persistence, 1.0f) {
}

PerlinF::PerlinF(IRandom &random, float octaves, float persistence, float expansion)
    : NoiseF() {
    mOctaves = octaves;
    mPersistence = persistence;
    mExpansion = expansion;
    mOffsetX = random.nextFloat() * 256.0f;
    mOffsetY = random.nextFloat() * 256.0f;
    mOffsetZ = random.nextFloat() * 256.0f;

    for (int32_t i = 0; i < 256; ++i) {
        mPerm[static_cast<size_t>(i)] = random.nextBoundedInt(255);
    }

    for (int32_t i = 0; i < 256; ++i) {
        const int32_t pos = random.nextBoundedInt(255 - i) + i;
        const int32_t old = mPerm[static_cast<size_t>(i)];
        mPerm[static_cast<size_t>(i)] = mPerm[static_cast<size_t>(pos)];
        mPerm[static_cast<size_t>(pos)] = old;
        mPerm[static_cast<size_t>(i + 256)] = mPerm[static_cast<size_t>(i)];
    }
}

float PerlinF::getValue(float x, float y, float z) const {
    return getNoise3D(x, y, z);
}

float PerlinF::getNoise2D(float x, float y) const {
    return getNoise3D(x, y, 0.0f);
}

float PerlinF::getNoise3D(float x, float y, float z) const {
    x += mOffsetX;
    y += mOffsetY;
    z += mOffsetZ;

    const int32_t floorX = static_cast<int32_t>(x);
    const int32_t floorY = static_cast<int32_t>(y);
    const int32_t floorZ = static_cast<int32_t>(z);

    const int32_t xIndex = floorX & 0xFF;
    const int32_t yIndex = floorY & 0xFF;
    const int32_t zIndex = floorZ & 0xFF;

    x -= static_cast<float>(floorX);
    y -= static_cast<float>(floorY);
    z -= static_cast<float>(floorZ);

    const float fX = x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
    const float fY = y * y * y * (y * (y * 6.0f - 15.0f) + 10.0f);
    const float fZ = z * z * z * (z * (z * 6.0f - 15.0f) + 10.0f);

    const int32_t a = mPerm[static_cast<size_t>(xIndex)] + yIndex;
    const int32_t b = mPerm[static_cast<size_t>(xIndex + 1)] + yIndex;

    const int32_t aa = mPerm[static_cast<size_t>(a)] + zIndex;
    const int32_t ab = mPerm[static_cast<size_t>(a + 1)] + zIndex;
    const int32_t ba = mPerm[static_cast<size_t>(b)] + zIndex;
    const int32_t bb = mPerm[static_cast<size_t>(b + 1)] + zIndex;

    const float aa1 = grad(mPerm[static_cast<size_t>(aa)], x, y, z);
    const float ba1 = grad(mPerm[static_cast<size_t>(ba)], x - 1.0f, y, z);
    const float ab1 = grad(mPerm[static_cast<size_t>(ab)], x, y - 1.0f, z);
    const float bb1 = grad(mPerm[static_cast<size_t>(bb)], x - 1.0f, y - 1.0f, z);
    const float aa2 = grad(mPerm[static_cast<size_t>(aa + 1)], x, y, z - 1.0f);
    const float ba2 = grad(mPerm[static_cast<size_t>(ba + 1)], x - 1.0f, y, z - 1.0f);
    const float ab2 = grad(mPerm[static_cast<size_t>(ab + 1)], x, y - 1.0f, z - 1.0f);
    const float bb2 = grad(mPerm[static_cast<size_t>(bb + 1)], x - 1.0f, y - 1.0f, z - 1.0f);

    const float xLerp11 = aa1 + fX * (ba1 - aa1);
    const float zLerp1 = xLerp11 + fY * (ab1 + fX * (bb1 - ab1) - xLerp11);
    const float xLerp21 = aa2 + fX * (ba2 - aa2);

    return zLerp1 + fZ * (xLerp21 + fY * (ab2 + fX * (bb2 - ab2) - xLerp21) - zLerp1);
}

SimplexF::SimplexF(IRandom &random, float octaves, float persistence)
    : PerlinF(random, octaves, persistence), mOffsetW(0.0f) {
    mOffsetW = random.nextFloat() * 256.0f;
}

SimplexF::SimplexF(IRandom &random, float octaves, float persistence, float expansion)
    : PerlinF(random, octaves, persistence, expansion), mOffsetW(0.0f) {
    mOffsetW = random.nextFloat() * 256.0f;
}

float SimplexF::dot2D(const int32_t *g, float x, float y) {
    return static_cast<float>(g[0]) * x + static_cast<float>(g[1]) * y;
}

float SimplexF::dot3D(const int32_t *g, float x, float y, float z) {
    return static_cast<float>(g[0]) * x + static_cast<float>(g[1]) * y + static_cast<float>(g[2]) * z;
}

float SimplexF::getNoise3D(float x, float y, float z) const {
    x += mOffsetX;
    y += mOffsetY;
    z += mOffsetZ;

    const float s = (x + y + z) * F3;
    const int32_t i = static_cast<int32_t>(x + s);
    const int32_t j = static_cast<int32_t>(y + s);
    const int32_t k = static_cast<int32_t>(z + s);
    const float t = static_cast<float>(i + j + k) * G3;

    const float x0 = x - (static_cast<float>(i) - t);
    const float y0 = y - (static_cast<float>(j) - t);
    const float z0 = z - (static_cast<float>(k) - t);

    int32_t i1 = 0;
    int32_t j1 = 0;
    int32_t k1 = 0;
    int32_t i2 = 0;
    int32_t j2 = 0;
    int32_t k2 = 0;

    if (x0 >= y0) {
        if (y0 >= z0) {
            i1 = 1;
            j1 = 0;
            k1 = 0;
            i2 = 1;
            j2 = 1;
            k2 = 0;
        } else if (x0 >= z0) {
            i1 = 1;
            j1 = 0;
            k1 = 0;
            i2 = 1;
            j2 = 0;
            k2 = 1;
        } else {
            i1 = 0;
            j1 = 0;
            k1 = 1;
            i2 = 1;
            j2 = 0;
            k2 = 1;
        }
    } else {
        if (y0 < z0) {
            i1 = 0;
            j1 = 0;
            k1 = 1;
            i2 = 0;
            j2 = 1;
            k2 = 1;
        } else if (x0 < z0) {
            i1 = 0;
            j1 = 1;
            k1 = 0;
            i2 = 0;
            j2 = 1;
            k2 = 1;
        } else {
            i1 = 0;
            j1 = 1;
            k1 = 0;
            i2 = 1;
            j2 = 1;
            k2 = 0;
        }
    }

    const float x1 = x0 - static_cast<float>(i1) + G3;
    const float y1 = y0 - static_cast<float>(j1) + G3;
    const float z1 = z0 - static_cast<float>(k1) + G3;
    const float x2 = x0 - static_cast<float>(i2) + 2.0f * G3;
    const float y2 = y0 - static_cast<float>(j2) + 2.0f * G3;
    const float z2 = z0 - static_cast<float>(k2) + 2.0f * G3;
    const float x3 = x0 - 1.0f + 3.0f * G3;
    const float y3 = y0 - 1.0f + 3.0f * G3;
    const float z3 = z0 - 1.0f + 3.0f * G3;

    const int32_t ii = i & 255;
    const int32_t jj = j & 255;
    const int32_t kk = k & 255;

    float n = 0.0f;

    const float t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;

    if (t0 > 0.0f) {
        const int32_t *gi0 = GRAD3[mPerm[static_cast<size_t>(ii + mPerm[static_cast<size_t>(jj + mPerm[static_cast<size_t>(kk)])])] % 12];
        n += t0 * t0 * t0 * t0 * (static_cast<float>(gi0[0]) * x0 + static_cast<float>(gi0[1]) * y0 + static_cast<float>(gi0[2]) * z0);
    }

    const float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;

    if (t1 > 0.0f) {
        const int32_t *gi1 = GRAD3[mPerm[static_cast<size_t>(ii + i1 + mPerm[static_cast<size_t>(jj + j1 + mPerm[static_cast<size_t>(kk + k1)])])] % 12];
        n += t1 * t1 * t1 * t1 * (static_cast<float>(gi1[0]) * x1 + static_cast<float>(gi1[1]) * y1 + static_cast<float>(gi1[2]) * z1);
    }

    const float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;

    if (t2 > 0.0f) {
        const int32_t *gi2 = GRAD3[mPerm[static_cast<size_t>(ii + i2 + mPerm[static_cast<size_t>(jj + j2 + mPerm[static_cast<size_t>(kk + k2)])])] % 12];
        n += t2 * t2 * t2 * t2 * (static_cast<float>(gi2[0]) * x2 + static_cast<float>(gi2[1]) * y2 + static_cast<float>(gi2[2]) * z2);
    }

    const float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;

    if (t3 > 0.0f) {
        const int32_t *gi3 = GRAD3[mPerm[static_cast<size_t>(ii + 1 + mPerm[static_cast<size_t>(jj + 1 + mPerm[static_cast<size_t>(kk + 1)])])] % 12];
        n += t3 * t3 * t3 * t3 * (static_cast<float>(gi3[0]) * x3 + static_cast<float>(gi3[1]) * y3 + static_cast<float>(gi3[2]) * z3);
    }

    return 32.0f * n;
}

float SimplexF::getNoise2D(float x, float y) const {
    x += mOffsetX;
    y += mOffsetY;

    const float s = (x + y) * F2;
    const int32_t i = static_cast<int32_t>(x + s);
    const int32_t j = static_cast<int32_t>(y + s);
    const float t = static_cast<float>(i + j) * G2;

    const float x0 = x - (static_cast<float>(i) - t);
    const float y0 = y - (static_cast<float>(j) - t);

    int32_t i1 = 0;
    int32_t j1 = 0;

    if (x0 > y0) {
        i1 = 1;
        j1 = 0;
    } else {
        i1 = 0;
        j1 = 1;
    }

    const float x1 = x0 - static_cast<float>(i1) + G2;
    const float y1 = y0 - static_cast<float>(j1) + G2;
    const float x2 = x0 + G22;
    const float y2 = y0 + G22;

    const int32_t ii = i & 255;
    const int32_t jj = j & 255;

    float n = 0.0f;

    const float t0 = 0.5f - x0 * x0 - y0 * y0;

    if (t0 > 0.0f) {
        const int32_t *gi0 = GRAD3[mPerm[static_cast<size_t>(ii + mPerm[static_cast<size_t>(jj)])] % 12];
        n += t0 * t0 * t0 * t0 * (static_cast<float>(gi0[0]) * x0 + static_cast<float>(gi0[1]) * y0);
    }

    const float t1 = 0.5f - x1 * x1 - y1 * y1;

    if (t1 > 0.0f) {
        const int32_t *gi1 = GRAD3[mPerm[static_cast<size_t>(ii + i1 + mPerm[static_cast<size_t>(jj + j1)])] % 12];
        n += t1 * t1 * t1 * t1 * (static_cast<float>(gi1[0]) * x1 + static_cast<float>(gi1[1]) * y1);
    }

    const float t2 = 0.5f - x2 * x2 - y2 * y2;

    if (t2 > 0.0f) {
        const int32_t *gi2 = GRAD3[mPerm[static_cast<size_t>(ii + 1 + mPerm[static_cast<size_t>(jj + 1)])] % 12];
        n += t2 * t2 * t2 * t2 * (static_cast<float>(gi2[0]) * x2 + static_cast<float>(gi2[1]) * y2);
    }

    return 70.0f * n;
}
