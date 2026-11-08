#pragma once

#include <string>
#include <vector>

class MobActor;

enum class NavigationMode {
    Walk,
    Climb,
    Swim,
    Fly
};

// from the active minecraft:navigation.* component; defaults match the component's
struct PathOptions {
    NavigationMode mMode = NavigationMode::Walk;
    bool mCanOpenDoors = false;
    bool mCanBreakDoors = false;
    bool mCanPassDoors = true;
    bool mAvoidSun = false;
    bool mAvoidWater = false;
    bool mAvoidDamageBlocks = false;
    bool mAvoidPortals = false;
    bool mCanPathOverWater = false;
    bool mCanPathOverLava = false;
    bool mCanWalkInLava = false;
    bool mCanSwim = false;
    bool mCanWalk = true;
    bool mCanSink = true;
    bool mCanFloat = false;
    bool mCanBreach = false;
    bool mCanJump = true;
    bool mCanPathFromAir = false;
    bool mIsAmphibious = false;
    std::vector<std::string> mBlocksToAvoid;

    static PathOptions read(const MobActor &mob);

    bool isVolumetric() const {
        return mMode == NavigationMode::Swim || mMode == NavigationMode::Fly;
    }

    bool swimsThroughWater() const {
        return mCanSwim || mIsAmphibious || mMode == NavigationMode::Swim;
    }
};
