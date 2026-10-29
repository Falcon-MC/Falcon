#pragma once

#include <cstdint>

class NoiseMath {
public:
    static double grad(int32_t hash, double x, double y, double z);

    static int64_t lfloor(double d);

    static int32_t floor(double d);

    static double lerp(double delta, double start, double end);

    static double lerp2(double deltaX, double deltaY, double val00, double val10, double val01, double val11);

    static double lerp3(double deltaX, double deltaY, double deltaZ, double val000, double val100, double val010, double val110, double val001, double val101, double val011, double val111);

    static double smoothStep(double d);

    static double maintainPrecision(double d);
};
