#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"

#include "World/Level/LevelGen/Overworld/Feature/Decoration/AzaleaRootSystemFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/BambooFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/CactusFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/CoralCrustFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/CoralHangFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/CoralTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DeadBushFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DoublePlantFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DryGrassDiscFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/EyeblossomFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/FireflyBushClusterFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/FireflyBushWaterClusterFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/FlowerBedFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/FlowerForestFoliageFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/ForestFlowerFoliageFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/HugeMushroomFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IcePatchFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IceSpikeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacySmallBrownMushroomsFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacySmallMushroomsFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/MesaFoliageFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/MonsterRoomFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/PaleMossPatchFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/PumpkinFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/ReedsFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SculkPatchFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SeaAnemoneFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SeaPickleFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IceCavesSnowLayerFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IceCavesSurfaceFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IceCrystalScatterFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IcicleClusterFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/LargeIcicleFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SwampFlowerDiscFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SweetBerryBushFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/TaigaGrassFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/WaterlilyFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/WildflowersBedFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Multi/OverworldCaveCarverAggregateFeature.h"

void registerDecorationFeatures() {
    GenerateFeatureRegistry::registerFeature(std::make_shared<AzaleaRootSystemFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<BambooFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CactusFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CoralCrustFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CoralHangFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CoralTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DeadBushFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DoublePlantFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DryGrassDiscFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<EyeblossomFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<FireflyBushClusterFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<FireflyBushWaterClusterFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<FlowerBedFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<FlowerForestFoliageFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<ForestFlowerFoliageFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<HugeMushroomFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IcePatchFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IceSpikeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacySmallBrownMushroomsFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacySmallMushroomsFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MesaFoliageFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MonsterRoomFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<PaleMossPatchFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<PumpkinFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<ReedsFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SculkPatchFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SeaAnemoneFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SeaPickleFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IceCavesSurfaceFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IceCrystalScatterFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IcicleClusterFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LargeIcicleFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IceCavesSnowLayerFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SwampFlowerDiscFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SweetBerryBushFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<TaigaGrassFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<WaterlilyFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<WildflowersBedFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<OverworldCaveCarverAggregateFeature>());
}
