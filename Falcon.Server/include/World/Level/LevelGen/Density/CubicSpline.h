#pragma once

#include "World/Level/LevelGen/Density/DensityFunction.h"

#include <cstdint>
#include <vector>

struct SplinePoint {
    double mLocation;
    DensityFunctionPtr mFunction;
    double mConstant;
    double mDerivative;
};

class CubicSpline {
public:
    CubicSpline(DensityFunctionPtr coordinate, std::vector<SplinePoint> points);

    double apply(FunctionContext &context);

    double minValue() const;

    double maxValue() const;

private:
    static int32_t _findRangeForLocation(const std::vector<double> &locations, double x);

    double _valueAt(size_t index, FunctionContext &context);

    DensityFunctionPtr mCoordinate;
    std::vector<double> mLocations;
    std::vector<SplinePoint> mValues;
    std::vector<double> mDerivatives;
    double mMinValue;
    double mMaxValue;
};

using CubicSplinePtr = std::shared_ptr<CubicSpline>;
