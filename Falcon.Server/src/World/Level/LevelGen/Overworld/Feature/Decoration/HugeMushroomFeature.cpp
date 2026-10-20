#include "World/Level/LevelGen/Overworld/Feature/Decoration/HugeMushroomFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"

namespace {

    class NullObjectGenerator : public ObjectGenerator {
    public:
        bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override {
            (void) manager;
            (void) random;
            (void) x;
            (void) y;
            (void) z;
            return false;
        }
    };

    ObjectGeneratorPtr (*gFactory)() = nullptr;

}

void HugeMushroomFeature::setGeneratorFactory(ObjectGeneratorPtr (*factory)()) {
    gFactory = factory;
}

const char *HugeMushroomFeature::name() const {
    return "minecraft:huge_mushroom_feature";
}

ObjectGeneratorPtr HugeMushroomFeature::getGenerator(IRandom &random) {
    (void) random;

    if (gFactory != nullptr)
        return gFactory();

    return std::make_shared<NullObjectGenerator>();
}

bool HugeMushroomFeature::canSpawnHere(int32_t biomeId) const {
    return DecorationSupport::isBiomeRoofed(biomeId)
           || DecorationSupport::isBiomeSwamp(biomeId)
           || DecorationSupport::isBiomeMooshroomIsland(biomeId);
}

int32_t HugeMushroomFeature::getMin() const {
    return -4;
}

int32_t HugeMushroomFeature::getMax() const {
    return 1;
}
