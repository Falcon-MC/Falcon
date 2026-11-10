#pragma once

#include <cstdint>
#include <memory>
#include <vector>

// float spline extrapolating along the end derivatives past its points, like the game (sub_1471C1360)
class TerrainSpline {
public:
    enum class Coordinate : uint8_t { Continents, Erosion, RidgesFolded, Ridges };

    // the climate a spline reads, in this order
    struct Climate {
        float mContinents;
        float mErosion;
        float mRidgesFolded;
        float mRidges;
    };

    struct Point {
        float mLocation;
        float mConstant;
        std::shared_ptr<const TerrainSpline> mNested;
        float mDerivative;
    };

    TerrainSpline(Coordinate coordinate, std::vector<Point> points);

    float apply(const Climate &climate) const;

    static std::shared_ptr<const TerrainSpline> overworldOffset();

    static std::shared_ptr<const TerrainSpline> overworldFactor();

    static std::shared_ptr<const TerrainSpline> overworldJaggedness();

private:
    float _valueAt(size_t index, const Climate &climate) const;

    Coordinate mCoordinate;
    std::vector<Point> mPoints;
};

using TerrainSplinePtr = std::shared_ptr<const TerrainSpline>;
