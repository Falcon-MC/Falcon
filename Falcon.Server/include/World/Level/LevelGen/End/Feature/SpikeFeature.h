#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

#include <cstdint>
#include <vector>

struct EndSpikeData {
    int32_t mX;
    int32_t mZ;
    int32_t mRadius;
    int32_t mHeight;
    bool mGuarded;
};

class SpikeFeature : public IFeature {
public:
    explicit SpikeFeature(int64_t seed);

    ~SpikeFeature() override = default;

    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

    static std::vector<EndSpikeData> createSpikes(int64_t seed);

private:
    std::vector<EndSpikeData> mSpikes;
};
