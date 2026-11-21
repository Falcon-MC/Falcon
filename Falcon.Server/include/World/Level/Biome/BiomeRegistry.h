#pragma once

#include "Protocol/Types/BiomeDefinitionData.h"

#include <string>
#include <vector>

class BiomeRegistry {
public:
    BiomeRegistry();

    const std::vector<BiomeDefinitionData> &getBiomes() const;

    const std::vector<std::string> &getStrings() const;

private:
    std::vector<BiomeDefinitionData> mBiomes;
    std::vector<std::string> mStrings;
};
