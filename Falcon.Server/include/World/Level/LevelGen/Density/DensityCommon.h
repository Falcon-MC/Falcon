#pragma once

#include "World/Level/LevelGen/Density/CubicSpline.h"
#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Density/NoiseHolder.h"

#include <cstdint>
#include <vector>

enum class MarkerType {
    INTERPOLATED,
    FLAT_CACHE,
    CACHE_2D,
    CACHE_ONCE,
    CACHE_ALL_IN_CELL
};

enum class MappedType {
    ABS,
    SQUARE,
    CUBE,
    HALF_NEGATIVE,
    QUARTER_NEGATIVE,
    INVERT,
    SQUEEZE
};

enum class TwoArgumentType {
    ADD,
    MUL,
    MIN,
    MAX
};

enum class RarityValueMapper {
    TYPE1,
    TYPE2
};

class TwoArgumentSimpleFunction {
public:
    static DensityFunctionPtr create(TwoArgumentType type, DensityFunctionPtr argument1, DensityFunctionPtr argument2);
};

class Mapped {
public:
    static DensityFunctionPtr create(MappedType type, DensityFunctionPtr input);
};

class Marker {
public:
    static DensityFunctionPtr create(MarkerType type, DensityFunctionPtr wrapped);
};

class DensityCommon {
public:
    static DensityFunctionPtr interpolated(DensityFunctionPtr function);

    static DensityFunctionPtr flatCache(DensityFunctionPtr function);

    static DensityFunctionPtr cache2d(DensityFunctionPtr function);

    static DensityFunctionPtr cacheOnce(DensityFunctionPtr function);

    static DensityFunctionPtr cacheAllInCell(DensityFunctionPtr function);

    static DensityFunctionPtr noise(const NormalNoisePtr &noise);

    static DensityFunctionPtr noise(const NormalNoisePtr &noise, double xzScale, double yScale);

    static DensityFunctionPtr noise(const NoiseHolder &noise, double xzScale, double yScale);

    static DensityFunctionPtr mappedNoise(const NormalNoisePtr &noise, double min, double max);

    static DensityFunctionPtr mappedNoise(const NormalNoisePtr &noise, double xzScale, double yScale, double min,
                                          double max);

    static DensityFunctionPtr weirdScaledSampler(DensityFunctionPtr input, const NormalNoisePtr &noise,
                                                 RarityValueMapper rarityValueMapper);

    static DensityFunctionPtr shiftA(const NormalNoisePtr &noise);

    static DensityFunctionPtr shiftB(const NormalNoisePtr &noise);

    static DensityFunctionPtr shift(const NormalNoisePtr &noise);

    static DensityFunctionPtr shiftedNoise(DensityFunctionPtr shiftX, DensityFunctionPtr shiftY,
                                           DensityFunctionPtr shiftZ, double xzScale, double yScale,
                                           const NoiseHolder &noise);

    static DensityFunctionPtr blendAlpha();

    static DensityFunctionPtr blendOffset();

    static DensityFunctionPtr blendDensity(DensityFunctionPtr input);

    static DensityFunctionPtr rangeChoice(DensityFunctionPtr input, double minInclusive, double maxExclusive,
                                          DensityFunctionPtr whenInRange, DensityFunctionPtr whenOutOfRange);

    static DensityFunctionPtr add(DensityFunctionPtr f1, DensityFunctionPtr f2);

    static DensityFunctionPtr mul(DensityFunctionPtr f1, DensityFunctionPtr f2);

    static DensityFunctionPtr min(DensityFunctionPtr f1, DensityFunctionPtr f2);

    static DensityFunctionPtr max(DensityFunctionPtr f1, DensityFunctionPtr f2);

    static DensityFunctionPtr zero();

    static DensityFunctionPtr constant(double value);

    static DensityFunctionPtr yClampedGradient(int32_t fromY, int32_t toY, double fromValue, double toValue);

    static DensityFunctionPtr map(DensityFunctionPtr function, MappedType type);

    static DensityFunctionPtr lerp(DensityFunctionPtr factor, double first, DensityFunctionPtr second);

    static DensityFunctionPtr spline(CubicSplinePtr spline);

    static DensityFunctionPtr spline(DensityFunctionPtr coordinate, std::vector<SplinePoint> points);

    static SplinePoint p(double location, double value, double derivative);

    static SplinePoint p(double location, DensityFunctionPtr value, double derivative);

    static DensityFunctionPtr clamp(DensityFunctionPtr input, double min, double max);

    static DensityFunctionPtr abs(DensityFunctionPtr input);

    static DensityFunctionPtr square(DensityFunctionPtr input);

    static DensityFunctionPtr cube(DensityFunctionPtr input);

    static DensityFunctionPtr halfNegative(DensityFunctionPtr input);

    static DensityFunctionPtr quarterNegative(DensityFunctionPtr input);

    static DensityFunctionPtr invert(DensityFunctionPtr input);

    static DensityFunctionPtr squeeze(DensityFunctionPtr input);

    static DensityFunctionPtr blockY();

private:
    static DensityFunctionPtr _mapFromUnitTo(DensityFunctionPtr input, double min, double max);
};
