#pragma once

struct Vector3f {
    float x;
    float y;
    float z;

    Vector3f() : x(0.0f), y(0.0f), z(0.0f) {
    }

    Vector3f(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue) {
    }

    bool operator==(const Vector3f &right) const {
        return x == right.x && y == right.y && z == right.z;
    }

    bool operator!=(const Vector3f &right) const {
        return !(*this == right);
    }
};

struct Vector2f {
    float x;
    float y;

    Vector2f() : x(0.0f), y(0.0f) {
    }

    Vector2f(float x, float y) : x(x), y(y) {
    }

    bool operator==(const Vector2f &right) const {
        return x == right.x && y == right.y;
    }

    bool operator!=(const Vector2f &right) const {
        return !(*this == right);
    }
};
