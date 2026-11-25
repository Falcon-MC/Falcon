#pragma once

#include <cstdint>

struct Vector3i {
    int32_t x;
    int32_t y;
    int32_t z;

    Vector3i() : x(0), y(0), z(0) {
    }

    Vector3i(int32_t xValue, int32_t yValue, int32_t zValue) : x(xValue), y(yValue), z(zValue) {
    }

    bool operator==(const Vector3i &right) const {
        return x == right.x && y == right.y && z == right.z;
    }

    bool operator!=(const Vector3i &right) const {
        return !(*this == right);
    }
};

struct Vector2i {
    int32_t x;
    int32_t y;

    Vector2i() : x(0), y(0) {
    }

    Vector2i(int32_t x, int32_t y) : x(x), y(y) {
    }

    bool operator==(const Vector2i &right) const {
        return x == right.x && y == right.y;
    }

    bool operator!=(const Vector2i &right) const {
        return !(*this == right);
    }
};
