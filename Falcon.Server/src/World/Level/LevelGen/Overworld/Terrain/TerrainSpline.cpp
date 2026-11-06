#include "World/Level/LevelGen/Overworld/Terrain/TerrainSpline.h"

#include <utility>

namespace {

    TerrainSpline::Point point(float location, float value, float derivative) {
        return TerrainSpline::Point{location, value, nullptr, derivative};
    }

    TerrainSpline::Point point(float location, TerrainSplinePtr value, float derivative) {
        return TerrainSpline::Point{location, 0.0f, std::move(value), derivative};
    }

    using Coordinate = TerrainSpline::Coordinate;

    TerrainSplinePtr spline(Coordinate coordinate, std::vector<TerrainSpline::Point> points) {
        return std::make_shared<TerrainSpline>(coordinate, std::move(points));
    }

}

TerrainSpline::TerrainSpline(Coordinate coordinate, std::vector<Point> points)
        : mCoordinate(coordinate), mPoints(std::move(points)) {
}

float TerrainSpline::apply(const Climate &climate) const {
    float x = climate.mContinents;
    if (mCoordinate == Coordinate::Erosion)
        x = climate.mErosion;
    else if (mCoordinate == Coordinate::RidgesFolded)
        x = climate.mRidgesFolded;
    else if (mCoordinate == Coordinate::Ridges)
        x = climate.mRidges;

    // first point whose location is above the coordinate
    size_t index = 0;
    size_t length = mPoints.size();
    while (length > 0) {
        const size_t half = length >> 1;
        if (mPoints[index + half].mLocation <= x) {
            index += half + 1;
            length -= half + 1;
        } else {
            length = half;
        }
    }

    if (index == 0)
        return (x - mPoints[0].mLocation) * mPoints[0].mDerivative + _valueAt(0, climate);

    const size_t count = mPoints.size();
    if (index == count) {
        const size_t last = count - 1;
        return (x - mPoints[last].mLocation) * mPoints[last].mDerivative + _valueAt(last, climate);
    }

    const Point &start = mPoints[index - 1];
    const Point &end = mPoints[index];
    const float startValue = _valueAt(index - 1, climate);
    const float endValue = _valueAt(index, climate);
    const float span = end.mLocation - start.mLocation;
    const float t = (x - start.mLocation) / span;
    const float rise = endValue - startValue;
    const float a = start.mDerivative * span - rise;
    const float b = rise - span * end.mDerivative;
    return ((b - a) * t + a) * ((1.0f - t) * t) + (t * rise + startValue);
}

float TerrainSpline::_valueAt(size_t index, const Climate &climate) const {
    const Point &point = mPoints[index];
    if (point.mNested != nullptr)
        return point.mNested->apply(climate);

    return point.mConstant;
}

TerrainSplinePtr TerrainSpline::overworldOffset() {
    const TerrainSplinePtr ridgesFolded1 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.08880186f, 0.38940096f),
        point(1.0f, 0.69000006f, 0.38940096f)
    });
    const TerrainSplinePtr ridgesFolded2 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.115760356f, 0.37788022f),
        point(1.0f, 0.6400001f, 0.37788022f)
    });
    const TerrainSplinePtr ridgesFolded3 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.2222f, 0.0f),
        point(-0.75f, -0.2222f, 0.0f),
        point(-0.65f, 0.0f, 0.0f),
        point(0.5954547f, 2.9802322E-8f, 0.0f),
        point(0.6054547f, 2.9802322E-8f, 0.2534563f),
        point(1.0f, 0.100000024f, 0.2534563f)
    });
    const TerrainSplinePtr ridgesFolded4 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.3f, 0.5f),
        point(-0.4f, 0.05f, 0.0f),
        point(0.0f, 0.05f, 0.0f),
        point(0.4f, 0.05f, 0.0f),
        point(1.0f, 0.060000002f, 0.007000001f)
    });
    const TerrainSplinePtr ridgesFolded5 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.15f, 0.5f),
        point(-0.4f, 0.0f, 0.0f),
        point(0.0f, 0.0f, 0.0f),
        point(0.4f, 0.05f, 0.1f),
        point(1.0f, 0.060000002f, 0.007000001f)
    });
    const TerrainSplinePtr ridgesFolded6 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.15f, 0.5f),
        point(-0.4f, 0.0f, 0.0f),
        point(0.0f, 0.0f, 0.0f),
        point(0.4f, 0.0f, 0.0f),
        point(1.0f, 0.0f, 0.0f)
    });
    const TerrainSplinePtr ridgesFolded7 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.02f, 0.0f),
        point(-0.4f, -0.03f, 0.0f),
        point(0.0f, -0.03f, 0.0f),
        point(0.4f, 0.0f, 0.06f),
        point(1.0f, 0.0f, 0.0f)
    });
    const TerrainSplinePtr ridgesFolded8 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.25f, 0.5f),
        point(-0.4f, 0.05f, 0.0f),
        point(0.0f, 0.05f, 0.0f),
        point(0.4f, 0.05f, 0.0f),
        point(1.0f, 0.060000002f, 0.007000001f)
    });
    const TerrainSplinePtr ridgesFolded9 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.1f, 0.5f),
        point(-0.4f, 0.001f, 0.01f),
        point(0.0f, 0.003f, 0.01f),
        point(0.4f, 0.05f, 0.094000004f),
        point(1.0f, 0.060000002f, 0.007000001f)
    });
    const TerrainSplinePtr ridgesFolded10 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.1f, 0.5f),
        point(-0.4f, 0.01f, 0.0f),
        point(0.0f, 0.01f, 0.0f),
        point(0.4f, 0.03f, 0.04f),
        point(1.0f, 0.1f, 0.049f)
    });
    const TerrainSplinePtr ridgesFolded11 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.02f, 0.0f),
        point(-0.4f, -0.03f, 0.0f),
        point(0.0f, -0.03f, 0.0f),
        point(0.4f, 0.03f, 0.12f),
        point(1.0f, 0.1f, 0.049f)
    });
    const TerrainSplinePtr ridgesFolded12 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, 0.20235021f, 0.0f),
        point(0.0f, 0.7161751f, 0.5138249f),
        point(1.0f, 1.23f, 0.5138249f)
    });
    const TerrainSplinePtr ridgesFolded13 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, 0.2f, 0.0f),
        point(0.0f, 0.44682026f, 0.43317974f),
        point(1.0f, 0.88f, 0.43317974f)
    });
    const TerrainSplinePtr ridgesFolded14 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, 0.2f, 0.0f),
        point(0.0f, 0.30829495f, 0.3917051f),
        point(1.0f, 0.70000005f, 0.3917051f)
    });
    const TerrainSplinePtr ridgesFolded15 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.25f, 0.5f),
        point(-0.4f, 0.35f, 0.0f),
        point(0.0f, 0.35f, 0.0f),
        point(0.4f, 0.35f, 0.0f),
        point(1.0f, 0.42000002f, 0.049000014f)
    });
    const TerrainSplinePtr ridgesFolded16 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.1f, 0.5f),
        point(-0.4f, 0.0069999998f, 0.07f),
        point(0.0f, 0.021f, 0.07f),
        point(0.4f, 0.35f, 0.658f),
        point(1.0f, 0.42000002f, 0.049000014f)
    });
    const TerrainSplinePtr ridgesFolded17 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.1f, 0.5f),
        point(-0.4f, 0.01f, 0.0f),
        point(0.0f, 0.01f, 0.0f),
        point(0.4f, 0.03f, 0.04f),
        point(1.0f, 0.1f, 0.049f)
    });
    const TerrainSplinePtr ridgesFolded19 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, 0.2f, 0.0f),
        point(0.0f, 0.5391705f, 0.4608295f),
        point(1.0f, 1.0f, 0.4608295f)
    });
    const TerrainSplinePtr ridgesFolded20 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.2f, 0.5f),
        point(-0.4f, 0.5f, 0.0f),
        point(0.0f, 0.5f, 0.0f),
        point(0.4f, 0.5f, 0.0f),
        point(1.0f, 0.6f, 0.070000015f)
    });
    const TerrainSplinePtr ridgesFolded21 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.05f, 0.5f),
        point(-0.4f, 0.01f, 0.099999994f),
        point(0.0f, 0.03f, 0.099999994f),
        point(0.4f, 0.5f, 0.94f),
        point(1.0f, 0.6f, 0.070000015f)
    });
    const TerrainSplinePtr ridgesFolded22 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.05f, 0.5f),
        point(-0.4f, 0.01f, 0.0f),
        point(0.0f, 0.01f, 0.0f),
        point(0.4f, 0.03f, 0.04f),
        point(1.0f, 0.1f, 0.049f)
    });
    const TerrainSplinePtr ridgesFolded23 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.02f, 0.015f),
        point(-0.4f, 0.01f, 0.0f),
        point(0.0f, 0.01f, 0.0f),
        point(0.4f, 0.03f, 0.04f),
        point(1.0f, 0.1f, 0.049f)
    });
    const TerrainSplinePtr ridgesFolded24 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, 0.34792626f, 0.0f),
        point(0.0f, 0.9239631f, 0.5760369f),
        point(1.0f, 1.5f, 0.5760369f)
    });
    const TerrainSplinePtr ridgesFolded25 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.1f, 0.0f),
        point(-0.4f, ridgesFolded17, 0.0f),
        point(0.0f, 0.17f, 0.0f)
    });
    const TerrainSplinePtr ridgesFolded26 = spline(Coordinate::RidgesFolded, {
        point(-1.0f, -0.05f, 0.0f),
        point(-0.4f, ridgesFolded22, 0.0f),
        point(0.0f, 0.17f, 0.0f)
    });
    const TerrainSplinePtr erosion1 = spline(Coordinate::Erosion, {
        point(-0.85f, ridgesFolded1, 0.0f),
        point(-0.7f, ridgesFolded2, 0.0f),
        point(-0.4f, ridgesFolded3, 0.0f),
        point(-0.35f, ridgesFolded4, 0.0f),
        point(-0.1f, ridgesFolded5, 0.0f),
        point(0.2f, ridgesFolded6, 0.0f),
        point(0.7f, ridgesFolded7, 0.0f)
    });
    const TerrainSplinePtr erosion2 = spline(Coordinate::Erosion, {
        point(-0.85f, ridgesFolded1, 0.0f),
        point(-0.7f, ridgesFolded2, 0.0f),
        point(-0.4f, ridgesFolded3, 0.0f),
        point(-0.35f, ridgesFolded8, 0.0f),
        point(-0.1f, ridgesFolded9, 0.0f),
        point(0.2f, ridgesFolded10, 0.0f),
        point(0.7f, ridgesFolded11, 0.0f)
    });
    const TerrainSplinePtr erosion3 = spline(Coordinate::Erosion, {
        point(-0.85f, ridgesFolded12, 0.0f),
        point(-0.7f, ridgesFolded13, 0.0f),
        point(-0.4f, ridgesFolded14, 0.0f),
        point(-0.35f, ridgesFolded15, 0.0f),
        point(-0.1f, ridgesFolded16, 0.0f),
        point(0.2f, ridgesFolded17, 0.0f),
        point(0.4f, ridgesFolded17, 0.0f),
        point(0.45f, ridgesFolded25, 0.0f),
        point(0.55f, ridgesFolded25, 0.0f),
        point(0.58f, ridgesFolded17, 0.0f),
        point(0.7f, ridgesFolded11, 0.0f)
    });
    const TerrainSplinePtr erosion4 = spline(Coordinate::Erosion, {
        point(-0.85f, ridgesFolded24, 0.0f),
        point(-0.7f, ridgesFolded19, 0.0f),
        point(-0.4f, ridgesFolded19, 0.0f),
        point(-0.35f, ridgesFolded20, 0.0f),
        point(-0.1f, ridgesFolded21, 0.0f),
        point(0.2f, ridgesFolded22, 0.0f),
        point(0.4f, ridgesFolded22, 0.0f),
        point(0.45f, ridgesFolded26, 0.0f),
        point(0.55f, ridgesFolded26, 0.0f),
        point(0.58f, ridgesFolded22, 0.0f),
        point(0.7f, ridgesFolded23, 0.0f)
    });
    return spline(Coordinate::Continents, {
        point(-1.1f, 0.044f, 0.0f),
        point(-1.02f, -0.2222f, 0.0f),
        point(-0.51f, -0.2222f, 0.0f),
        point(-0.44f, -0.12f, 0.0f),
        point(-0.18f, -0.12f, 0.0f),
        point(-0.16f, erosion1, 0.0f),
        point(-0.15f, erosion1, 0.0f),
        point(-0.1f, erosion2, 0.0f),
        point(0.25f, erosion3, 0.0f),
        point(1.0f, erosion4, 0.0f)
    });
}

TerrainSplinePtr TerrainSpline::overworldFactor() {
    const TerrainSplinePtr ridgesA = spline(Coordinate::Ridges, {
        point(-0.2f, 6.3f, 0.0f),
        point(0.2f, 6.25f, 0.0f)
    });
    const TerrainSplinePtr ridgesB = spline(Coordinate::Ridges, {
        point(-0.05f, 6.3f, 0.0f),
        point(0.05f, 2.67f, 0.0f)
    });
    const TerrainSplinePtr ridgesC = spline(Coordinate::Ridges, {
        point(-0.05f, 2.67f, 0.0f),
        point(0.05f, 6.3f, 0.0f)
    });
    const TerrainSplinePtr ridgesD = spline(Coordinate::Ridges, {
        point(-0.2f, 6.3f, 0.0f),
        point(0.2f, 5.47f, 0.0f)
    });
    const TerrainSplinePtr ridgesE = spline(Coordinate::Ridges, {
        point(-0.2f, 6.3f, 0.0f),
        point(0.2f, 5.08f, 0.0f)
    });
    const TerrainSplinePtr ridgesF = spline(Coordinate::Ridges, {
        point(-0.2f, 6.3f, 0.0f),
        point(0.2f, 4.69f, 0.0f)
    });
    const TerrainSplinePtr ridgesG = spline(Coordinate::Ridges, {
        point(0.0f, 6.25f, 0.0f),
        point(0.1f, 0.625f, 0.0f)
    });
    const TerrainSplinePtr ridgesH = spline(Coordinate::Ridges, {
        point(0.0f, 5.47f, 0.0f),
        point(0.1f, 0.625f, 0.0f)
    });
    const TerrainSplinePtr ridgesI = spline(Coordinate::Ridges, {
        point(0.0f, 5.08f, 0.0f),
        point(0.1f, 0.625f, 0.0f)
    });
    const TerrainSplinePtr ridgesFoldedA = spline(Coordinate::RidgesFolded, {
        point(-0.9f, 6.25f, 0.0f),
        point(-0.69f, ridgesG, 0.0f)
    });
    const TerrainSplinePtr ridgesFoldedB = spline(Coordinate::RidgesFolded, {
        point(-0.9f, 5.47f, 0.0f),
        point(-0.69f, ridgesH, 0.0f)
    });
    const TerrainSplinePtr ridgesFoldedC = spline(Coordinate::RidgesFolded, {
        point(-0.9f, 5.08f, 0.0f),
        point(-0.69f, ridgesI, 0.0f)
    });
    const TerrainSplinePtr ridgesFoldedD = spline(Coordinate::RidgesFolded, {
        point(0.45f, ridgesF, 0.0f),
        point(0.7f, 1.56f, 0.0f)
    });
    const TerrainSplinePtr ridgesFoldedE = spline(Coordinate::RidgesFolded, {
        point(-0.7f, ridgesF, 0.0f),
        point(-0.15f, 1.37f, 0.0f)
    });
    const TerrainSplinePtr erosion1 = spline(Coordinate::Erosion, {
        point(-0.6f, ridgesA, 0.0f),
        point(-0.5f, ridgesB, 0.0f),
        point(-0.35f, ridgesA, 0.0f),
        point(-0.25f, ridgesA, 0.0f),
        point(-0.1f, ridgesC, 0.0f),
        point(0.03f, ridgesA, 0.0f),
        point(0.35f, 6.25f, 0.0f),
        point(0.45f, ridgesFoldedA, 0.0f),
        point(0.55f, ridgesFoldedA, 0.0f),
        point(0.62f, 6.25f, 0.0f)
    });
    const TerrainSplinePtr erosion2 = spline(Coordinate::Erosion, {
        point(-0.6f, ridgesD, 0.0f),
        point(-0.5f, ridgesB, 0.0f),
        point(-0.35f, ridgesD, 0.0f),
        point(-0.25f, ridgesD, 0.0f),
        point(-0.1f, ridgesC, 0.0f),
        point(0.03f, ridgesD, 0.0f),
        point(0.35f, 5.47f, 0.0f),
        point(0.45f, ridgesFoldedB, 0.0f),
        point(0.55f, ridgesFoldedB, 0.0f),
        point(0.62f, 5.47f, 0.0f)
    });
    const TerrainSplinePtr erosion3 = spline(Coordinate::Erosion, {
        point(-0.6f, ridgesE, 0.0f),
        point(-0.5f, ridgesB, 0.0f),
        point(-0.35f, ridgesE, 0.0f),
        point(-0.25f, ridgesE, 0.0f),
        point(-0.1f, ridgesC, 0.0f),
        point(0.03f, ridgesE, 0.0f),
        point(0.35f, 5.08f, 0.0f),
        point(0.45f, ridgesFoldedC, 0.0f),
        point(0.55f, ridgesFoldedC, 0.0f),
        point(0.62f, 5.08f, 0.0f)
    });
    const TerrainSplinePtr erosion4 = spline(Coordinate::Erosion, {
        point(-0.6f, ridgesF, 0.0f),
        point(-0.5f, ridgesB, 0.0f),
        point(-0.35f, ridgesF, 0.0f),
        point(-0.25f, ridgesF, 0.0f),
        point(-0.1f, ridgesC, 0.0f),
        point(0.03f, ridgesF, 0.0f),
        point(0.05f, ridgesFoldedD, 0.0f),
        point(0.4f, ridgesFoldedD, 0.0f),
        point(0.45f, ridgesFoldedE, 0.0f),
        point(0.55f, ridgesFoldedE, 0.0f),
        point(0.58f, 4.69f, 0.0f)
    });
    return spline(Coordinate::Continents, {
        point(-0.19f, 3.95f, 0.0f),
        point(-0.15f, erosion1, 0.0f),
        point(-0.1f, erosion2, 0.0f),
        point(0.03f, erosion3, 0.0f),
        point(0.06f, erosion4, 0.0f)
    });
}

TerrainSplinePtr TerrainSpline::overworldJaggedness() {
    const TerrainSplinePtr ridges1 = spline(Coordinate::Ridges, {
        point(-0.01f, 0.63f, 0.0f),
        point(0.01f, 0.3f, 0.0f)
    });
    const TerrainSplinePtr ridges2 = spline(Coordinate::Ridges, {
        point(-0.01f, 0.315f, 0.0f),
        point(0.01f, 0.15f, 0.0f)
    });
    const TerrainSplinePtr ridges3 = spline(Coordinate::Ridges, {
        point(-0.01f, 0.315f, 0.0f),
        point(0.01f, 0.15f, 0.0f)
    });
    const TerrainSplinePtr ridges4 = spline(Coordinate::Ridges, {
        point(-0.01f, 0.63f, 0.0f),
        point(0.01f, 0.3f, 0.0f)
    });
    const TerrainSplinePtr ridges5 = spline(Coordinate::Ridges, {
        point(-0.01f, 0.63f, 0.0f),
        point(0.01f, 0.3f, 0.0f)
    });
    const TerrainSplinePtr ridges6 = spline(Coordinate::Ridges, {
        point(-0.01f, 0.63f, 0.0f),
        point(0.01f, 0.3f, 0.0f)
    });
    const TerrainSplinePtr ridgesFolded1 = spline(Coordinate::RidgesFolded, {
        point(0.19999999f, 0.0f, 0.0f),
        point(0.44999996f, 0.0f, 0.0f),
        point(1.0f, ridges1, 0.0f)
    });
    const TerrainSplinePtr ridgesFolded2 = spline(Coordinate::RidgesFolded, {
        point(0.19999999f, 0.0f, 0.0f),
        point(0.44999996f, 0.0f, 0.0f),
        point(1.0f, ridges2, 0.0f)
    });
    const TerrainSplinePtr ridgesFolded3 = spline(Coordinate::RidgesFolded, {
        point(0.19999999f, 0.0f, 0.0f),
        point(0.44999996f, 0.0f, 0.0f),
        point(1.0f, ridges3, 0.0f)
    });
    const TerrainSplinePtr ridgesFolded4 = spline(Coordinate::RidgesFolded, {
        point(0.19999999f, 0.0f, 0.0f),
        point(0.44999996f, ridges4, 0.0f),
        point(1.0f, ridges5, 0.0f)
    });
    const TerrainSplinePtr ridgesFolded5 = spline(Coordinate::RidgesFolded, {
        point(0.19999999f, 0.0f, 0.0f),
        point(0.44999996f, 0.0f, 0.0f),
        point(1.0f, ridges6, 0.0f)
    });
    const TerrainSplinePtr erosion1 = spline(Coordinate::Erosion, {
        point(-1.0f, ridgesFolded1, 0.0f),
        point(-0.78f, ridgesFolded2, 0.0f),
        point(-0.5775f, ridgesFolded3, 0.0f),
        point(-0.375f, 0.0f, 0.0f)
    });
    const TerrainSplinePtr erosion2 = spline(Coordinate::Erosion, {
        point(-1.0f, ridgesFolded4, 0.0f),
        point(-0.78f, ridgesFolded5, 0.0f),
        point(-0.5775f, ridgesFolded5, 0.0f),
        point(-0.375f, 0.0f, 0.0f)
    });
    return spline(Coordinate::Continents, {
        point(-0.11f, 0.0f, 0.0f),
        point(0.03f, erosion1, 0.0f),
        point(0.65f, erosion2, 0.0f)
    });
}
