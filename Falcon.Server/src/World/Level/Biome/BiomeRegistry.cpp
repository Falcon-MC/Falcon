#include "World/Level/Biome/BiomeRegistry.h"

#include "Core/Debug/BedrockLog.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"

BiomeRegistry::BiomeRegistry()
    : mBiomes(BiomeChunkGenDataRegistry::getDefinitions()), mStrings(BiomeChunkGenDataRegistry::getDefinitionStrings()) {
    if (mBiomes.empty())
        LOG_WARN(LogAreaID::Server, "No biome definitions were loaded, clients will draw biomes with default colours");
}

const std::vector<BiomeDefinitionData> &BiomeRegistry::getBiomes() const {
    return mBiomes;
}

const std::vector<std::string> &BiomeRegistry::getStrings() const {
    return mStrings;
}
