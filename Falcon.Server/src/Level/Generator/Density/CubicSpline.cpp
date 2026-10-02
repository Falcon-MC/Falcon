#include "Level/Generator/Density/CubicSpline.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

CubicSpline::CubicSpline(DensityFunctionPtr coordinate, std::vector<SplinePoint> points)
        : mCoordinate(std::move(coordinate)), mMinValue(0.0), mMaxValue(0.0) {
    if (points.size() < 2) {
        throw std::invalid_argument("CubicSpline needs at least two points");
    }

    std::stable_sort(points.begin(), points.end(), [](const SplinePoint &left, const SplinePoint &right) {
        return left.mLocation < right.mLocation;
    });

    size_t pointCount = points.size();
    mLocations.resize(pointCount);
    mValues.resize(pointCount);
    mDerivatives.resize(pointCount);

    double min = std::numeric_limits<double>::infinity();
    double max = -std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < pointCount; i++) {
        const SplinePoint &point = points[i];
        mLocations[i] = point.mLocation;
        mValues[i] = point;
        mDerivatives[i] = point.mDerivative;

        double valueMin = point.mFunction != nullptr ? point.mFunction->minValue() : point.mConstant;
        double valueMax = point.mFunction != nullptr ? point.mFunction->maxValue() : point.mConstant;
        min = std::min(min, valueMin);
        max = std::max(max, valueMax);
    }

    mMinValue = min;
    mMaxValue = max;
}

/**
 * Cubic Hermite interpolation between the two points around the coordinate, using each
 * point's stored derivative. Outside the first and last point the value is held constant
 * rather than extrapolated. Point values may themselves be density functions, which is how
 * splines nest.
 */
double CubicSpline::apply(FunctionContext &context) {
    double x = mCoordinate->compute(context);
    int32_t range = _findRangeForLocation(mLocations, x);
    if (range < 0) {
        return _valueAt(0, context);
    }

    int32_t last = static_cast<int32_t>(mLocations.size()) - 1;
    if (range == last) {
        return _valueAt(static_cast<size_t>(last), context);
    }

    double loc0 = mLocations[static_cast<size_t>(range)];
    double loc1 = mLocations[static_cast<size_t>(range) + 1];
    double locDist = loc1 - loc0;
    double k = (x - loc0) / locDist;
    double y0 = _valueAt(static_cast<size_t>(range), context);
    double y1 = _valueAt(static_cast<size_t>(range) + 1, context);
    double yDist = y1 - y0;
    double p = mDerivatives[static_cast<size_t>(range)] * locDist - yDist;
    double q = -mDerivatives[static_cast<size_t>(range) + 1] * locDist + yDist;
    return y0 + k * yDist + k * (1.0 - k) * (p + k * (q - p));
}

double CubicSpline::minValue() const {
    return mMinValue;
}

double CubicSpline::maxValue() const {
    return mMaxValue;
}

int32_t CubicSpline::_findRangeForLocation(const std::vector<double> &locations, double x) {
    int32_t min = 0;
    int32_t length = static_cast<int32_t>(locations.size());

    while (length > 0) {
        int32_t half = static_cast<int32_t>(static_cast<uint32_t>(length) >> 1);
        int32_t mid = min + half;
        if (x < locations[static_cast<size_t>(mid)]) {
            length = half;
        } else {
            min = mid + 1;
            length -= half + 1;
        }
    }

    return min - 1;
}

double CubicSpline::_valueAt(size_t index, FunctionContext &context) {
    const SplinePoint &point = mValues[index];
    if (point.mFunction != nullptr) {
        return point.mFunction->compute(context);
    }

    return point.mConstant;
}
