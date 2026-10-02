#include "Level/Generator/Overworld/Biome/OverworldBiomeResult.h"

#include "Level/Generator/Biome/BiomeIds.h"

OverworldBiomeResult::OverworldBiomeResult(int32_t biomeId, float continental, float temperature, float humidity,
                                           float erosion, float weirdness, float pv)
        : BiomeResult(biomeId),
          mContinental(continental),
          mTemperature(temperature),
          mHumidity(humidity),
          mErosion(erosion),
          mWeirdness(weirdness),
          mPv(pv),
          mOriginal(biomeId) {
}

OverworldBiomeResult &OverworldBiomeResult::correct(int32_t y) {
    const float depth = (float) (-y) / 128.0f;

    if (depth >= 0.2f) {
        if (depth < 0.99f) {
            if (mContinental > 0.8f && mContinental < 1.0f) {
                mBiomeId = BiomeIds::DRIPSTONE_CAVES;
            } else if (mTemperature > 0.55f && mHumidity < -0.1f) {
                mBiomeId = BiomeIds::SULFUR_CAVES;
            } else if (mTemperature < -0.45f) {
                mBiomeId = BiomeIds::ICE_CAVES;
            } else if (mHumidity > 0.3f) {
                mBiomeId = BiomeIds::LUSH_CAVES;
            }
        } else if (depth > 0.9f) {
            if (mErosion < -0.225f) {
                mBiomeId = BiomeIds::DEEP_DARK;
            }
        }
    }

    return *this;
}

void OverworldBiomeResult::reset() {
    mBiomeId = mOriginal;
}
