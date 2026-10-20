#pragma once

#include "Core/Math/Vector3f.h"

class MobActor;

class LookControl {
public:
    void setLookAt(const Vector3f &position);

    void clear();

    bool hasLookAt() const {
        return mHasLookAt;
    }

    void setPitchEnabled(bool enabled) {
        mPitchEnabled = enabled;
    }

    void tick(MobActor &mob);

    static float yawTowards(const Vector3f &from, const Vector3f &to);

    static float pitchTowards(const Vector3f &from, const Vector3f &to);

private:
    Vector3f mLookAt;
    bool mHasLookAt = false;
    bool mPitchEnabled = false;
};
