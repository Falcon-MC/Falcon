#include "World/Level/Biome/BiomeRegistry.h"

#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"

BiomeRegistry::BiomeRegistry()
    : mBiomes(BiomeChunkGenDataRegistry::getDefinitions()), mStrings(BiomeChunkGenDataRegistry::getDefinitionStrings()) {
}

const std::vector<BiomeDefinitionData> &BiomeRegistry::getBiomes() const {
    return mBiomes;
}

const std::vector<std::string> &BiomeRegistry::getStrings() const {
    return mStrings;
}
