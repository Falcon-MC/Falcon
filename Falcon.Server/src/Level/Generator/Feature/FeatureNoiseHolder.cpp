#include "Level/Generator/Feature/FeatureNoiseHolder.h"

#include <memory>
#include <vector>

namespace {
    std::unique_ptr<FeatureNoiseHolder> instance;
}

FeatureNoiseHolder::FeatureNoiseHolder(IRandom &random) {
    const IRandomPtr clayRandom = random.identical();
    mRandomClayWithDripleavesSnapToFloor =
            std::make_shared<SimplexF>(*clayRandom, 1.0f, 2.0f / 4.0f, 1.0f / 15.0f);

    const IRandomPtr dripstoneRandom = random.identical();
    mDripstoneCluster = std::make_shared<SimplexF>(*dripstoneRandom, 30.0f, 1.0f / 99.0f, 1.0f / 15.0f);

    const IRandomPtr mossFloorRandom = random.identical();
    mMossPatchSnapToFloor = std::make_shared<SimplexF>(*mossFloorRandom, 2.0f, 2.0f / 4.0f, 1.0f / 10.0f);

    const IRandomPtr mossCeilingRandom = random.identical();
    mMossSnapToCeiling = std::make_shared<SimplexF>(*mossCeilingRandom, 2.0f, 2.0f / 4.0f, 1.0f / 30.0f);

    const IRandomPtr sculkRandom = random.identical();
    mSculkPatch = std::make_shared<SimplexF>(*sculkRandom, 20.0f, 1.0f / 99.0f, 1.0f / 100.0f);

    const IRandomPtr kelpRandom = random.identical();
    mKelp = std::make_shared<SimplexNoise>(*kelpRandom, -7, std::vector<float>{1.0f});

    const IRandomPtr sulfurRandom = random.fork();
    mSulfurCaveGradient = std::make_shared<SimplexNoise>(*sulfurRandom, -5, std::vector<float>{1.0f, 0.0f, 1.0f});

    const IRandomPtr iceRandom = random.fork();
    mIceCaveGradient = std::make_shared<SimplexNoise>(*iceRandom, -4, std::vector<float>{1.0f, 0.0f, 1.0f});
}

void FeatureNoiseHolder::initialize(IRandom &random) {
    instance.reset(new FeatureNoiseHolder(random));
}

const FeatureNoiseHolder *FeatureNoiseHolder::get() {
    return instance.get();
}
