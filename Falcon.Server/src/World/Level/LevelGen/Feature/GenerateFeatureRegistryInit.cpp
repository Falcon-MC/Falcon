#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"

#include "World/Level/LevelGen/Overworld/Feature/Decoration/AzaleaRootSystemFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/HugeMushroomFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/AzaleaTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/HugeMushroom.h"

#include <memory>
#include <mutex>

void registerOreFeatures();

void registerTreeFeatures();

void registerDecorationFeatures();

namespace {
    ObjectGeneratorPtr createHugeMushroom() {
        return std::make_shared<HugeMushroom>();
    }

    ObjectGeneratorPtr createAzaleaTree() {
        return std::make_shared<AzaleaTree>();
    }

    std::mutex &registrationMutex() {
        static std::mutex mutex;
        return mutex;
    }
}

void GenerateFeatureRegistry::initialize() {
    static thread_local bool initialized = false;
    if (initialized)
        return;

    initialized = true;

    const std::lock_guard<std::mutex> guard(registrationMutex());

    HugeMushroomFeature::setGeneratorFactory(&createHugeMushroom);
    AzaleaRootSystemFeature::setGeneratorFactory(&createAzaleaTree);

    registerOreFeatures();
    registerTreeFeatures();
    registerDecorationFeatures();
}
