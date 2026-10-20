#pragma once

#include "Protocol/Types/BiomeDefinitionData.h"

#include <vector>

class BiomeRegistry {
public:
    BiomeRegistry();

    const std::vector<BiomeDefinitionData> &getBiomes() const;

private:
    std::vector<BiomeDefinitionData> mBiomes;
};
