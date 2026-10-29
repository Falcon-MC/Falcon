#include "World/Level/LevelGen/Noise/SimplexNoiseSampler.h"

#include "World/Level/LevelGen/Noise/NoiseMath.h"

#include <cmath>

namespace {

const double SQRT_3 = std::sqrt(3.0);
const double SKEW_FACTOR_2D = 0.5 * (SQRT_3 - 1.0);
const double UNSKEW_FACTOR_2D = (3.0 - SQRT_3) / 6.0;
constexpr double F3 = 0.3333333333333333;
constexpr double G3 = 0.16666666666666666;

}

SimplexNoiseSampler::SimplexNoiseSampler(IRandom &rand)
    : Noise(rand) {
}

double SimplexNoiseSampler::sample2D(double x, double y) const {
    const double hairyFactor = (x + y) * SKEW_FACTOR_2D;
    const int32_t hairyX = NoiseMath::floor(x + hairyFactor);
    const int32_t hairyZ = NoiseMath::floor(y + hairyFactor);
    const double mixedHairyXz = static_cast<double>(hairyX + hairyZ) * UNSKEW_FACTOR_2D;
    const double diffXToXz = static_cast<double>(hairyX) - mixedHairyXz;
    const double diffZToXz = static_cast<double>(hairyZ) - mixedHairyXz;
    const double x0 = x - diffXToXz;
    const double y0 = y - diffZToXz;

    int8_t offsetSecondCornerX = 0;
    int8_t offsetSecondCornerZ = 0;

    if (x0 > y0) {
        offsetSecondCornerX = 1;
        offsetSecondCornerZ = 0;
    } else {
        offsetSecondCornerX = 0;
        offsetSecondCornerZ = 1;
    }

    const double x1 = x0 - static_cast<double>(offsetSecondCornerX) + UNSKEW_FACTOR_2D;
    const double y1 = y0 - static_cast<double>(offsetSecondCornerZ) + UNSKEW_FACTOR_2D;
    const double x3 = x0 - 1.0 + 2.0 * UNSKEW_FACTOR_2D;
    const double y3 = y0 - 1.0 + 2.0 * UNSKEW_FACTOR_2D;

    const int32_t ii = hairyX & 255;
    const int32_t jj = hairyZ & 255;
    const int32_t gi0 = lookup(ii + lookup(jj)) % 12;
    const int32_t gi1 = lookup(ii + static_cast<int32_t>(offsetSecondCornerX) + lookup(jj + static_cast<int32_t>(offsetSecondCornerZ))) % 12;
    const int32_t gi2 = lookup(ii + 1 + lookup(jj + 1)) % 12;

    const double t0 = _cornerNoise3d(gi0, x0, y0, 0.0, 0.5);
    const double t1 = _cornerNoise3d(gi1, x1, y1, 0.0, 0.5);
    const double t2 = _cornerNoise3d(gi2, x3, y3, 0.0, 0.5);

    return 70.0 * (t0 + t1 + t2);
}

double SimplexNoiseSampler::sample3D(double x, double y, double z) const {
    const double skewFactor = (x + y + z) * F3;
    const int32_t i = NoiseMath::floor(x + skewFactor);
    const int32_t j = NoiseMath::floor(y + skewFactor);
    const int32_t k = NoiseMath::floor(z + skewFactor);
    const double unskewFactor = static_cast<double>(i + j + k) * G3;

    double x0 = static_cast<double>(i) - unskewFactor;
    double y0 = static_cast<double>(j) - unskewFactor;
    double z0 = static_cast<double>(k) - unskewFactor;
    x0 = x - x0;
    y0 = y - y0;
    z0 = z - z0;

    int8_t i1 = 0;
    int8_t j1 = 0;
    int8_t k1 = 0;
    int8_t i2 = 0;
    int8_t j2 = 0;
    int8_t k2 = 0;

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
    } else if (y0 < z0) {
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

    const double x1 = x0 - static_cast<double>(i1) + G3;
    const double y1 = y0 - static_cast<double>(j1) + G3;
    const double z1 = z0 - static_cast<double>(k1) + G3;
    const double x2 = x0 - static_cast<double>(i2) + F3;
    const double y2 = y0 - static_cast<double>(j2) + F3;
    const double z2 = z0 - static_cast<double>(k2) + F3;
    const double x3 = x0 - 1.0 + 0.5;
    const double y3 = y0 - 1.0 + 0.5;
    const double z3 = z0 - 1.0 + 0.5;

    const int32_t ii = i & 255;
    const int32_t jj = j & 255;
    const int32_t kk = k & 255;

    const int32_t gi0 = lookup(ii + lookup(jj + lookup(kk))) % 12;
    const int32_t gi1 = lookup(ii + static_cast<int32_t>(i1) + lookup(jj + static_cast<int32_t>(j1) + lookup(kk + static_cast<int32_t>(k1)))) % 12;
    const int32_t gi2 = lookup(ii + static_cast<int32_t>(i2) + lookup(jj + static_cast<int32_t>(j2) + lookup(kk + static_cast<int32_t>(k2)))) % 12;
    const int32_t gi3 = lookup(ii + 1 + lookup(jj + 1 + lookup(kk + 1))) % 12;

    const double t0 = _cornerNoise3d(gi0, x0, y0, z0, 0.6);
    const double t1 = _cornerNoise3d(gi1, x1, y1, z1, 0.6);
    const double t2 = _cornerNoise3d(gi2, x2, y2, z2, 0.6);
    const double t3 = _cornerNoise3d(gi3, x3, y3, z3, 0.6);

    return 32.0 * (t0 + t1 + t2 + t3);
}

double SimplexNoiseSampler::_cornerNoise3d(int32_t hash, double x, double y, double z, double max) const {
    double contribution = max - x * x - y * y - z * z;
    double result = 0.0;

    if (contribution < 0.0) {
        result = 0.0;
    } else {
        contribution *= contribution;
        result = contribution * contribution * NoiseMath::grad(hash, x, y, z);
    }

    return result;
}
