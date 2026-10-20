#pragma once

#include "World/Level/LevelGen/Noise/SimplexF.h"
#include "World/Level/LevelGen/Noise/SimplexNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <memory>

class FeatureNoiseHolder {
public:
    static void initialize(IRandom &random);

    static const FeatureNoiseHolder *get();

    const SimplexF &getRandomClayWithDripleavesSnapToFloor() const { return *mRandomClayWithDripleavesSnapToFloor; }

    const SimplexF &getDripstoneCluster() const { return *mDripstoneCluster; }

    const SimplexF &getMossPatchSnapToFloor() const { return *mMossPatchSnapToFloor; }

    const SimplexF &getMossSnapToCeiling() const { return *mMossSnapToCeiling; }

    const SimplexF &getSculkPatch() const { return *mSculkPatch; }

    const SimplexNoise &getKelp() const { return *mKelp; }

    const SimplexNoise &getSulfurCaveGradient() const { return *mSulfurCaveGradient; }

private:
    explicit FeatureNoiseHolder(IRandom &random);

    std::shared_ptr<SimplexF> mRandomClayWithDripleavesSnapToFloor;
    std::shared_ptr<SimplexF> mDripstoneCluster;
    std::shared_ptr<SimplexF> mMossPatchSnapToFloor;
    std::shared_ptr<SimplexF> mMossSnapToCeiling;
    std::shared_ptr<SimplexF> mSculkPatch;
    std::shared_ptr<SimplexNoise> mKelp;
    std::shared_ptr<SimplexNoise> mSulfurCaveGradient;
};
