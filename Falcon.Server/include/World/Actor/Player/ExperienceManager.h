#pragma once

#include "World/Actor/ActorAttributes.h"

#include <cstdint>

class ExperienceManager {
public:
    static const char *ATTRIBUTE_LEVEL;
    static const char *ATTRIBUTE_PROGRESS;

    static constexpr int MAX_LEVEL = 24791;

    static int getXpToReachLevel(int level);

    static int getXpToCompleteLevel(int level);

    static float getLevelFromXp(int xp);

    int getXpLevel() const { return mXpLevel; }

    void setXpLevel(int level);

    void addXpLevels(int amount);

    void subtractXpLevels(int amount);

    float getXpProgress() const { return mXpProgress; }

    void setXpProgress(float progress);

    void setXpAndProgress(int level, float progress);

    int getRemainderXp() const;

    int getCurrentTotalXp() const;

    void setCurrentTotalXp(int amount);

    void addXp(int amount);

    void subtractXp(int amount);

    int getLifetimeTotalXp() const { return mLifetimeTotalXp; }

    void setLifetimeTotalXp(int amount);

    int getXpDropAmount() const;

    void reset();

    void applyTo(ActorAttributes &attributes) const;

private:
    int mXpLevel = 0;
    float mXpProgress = 0.0f;
    int mLifetimeTotalXp = 0;
};
