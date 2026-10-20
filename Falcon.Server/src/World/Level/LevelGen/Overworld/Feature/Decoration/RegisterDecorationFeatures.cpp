#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"

#include "World/Level/LevelGen/Overworld/Feature/Decoration/AzaleaRootSystemFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/BambooFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/BushFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/CactusFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/ClayWithDripleavesPatchFeature.h"
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
#include "World/Level/LevelGen/Overworld/Feature/Decoration/GeodeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/HugeMushroomFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IcePatchFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IceSpikeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IcebergFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/JungleGrassFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/KelpFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/LargeFernDoublePlantFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacyFlowerFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacyForestFoliageFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacyForestRockFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacySmallBrownMushroomsFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacySmallMushroomsFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/MelonFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/MesaFoliageFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/MonsterRoomFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/MossCeilingPatchFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/MultifaceFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/OceanSeagrassFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/PaleMossPatchFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/PlainsFlowerFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/PumpkinFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/ReedsFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SculkPatchFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SeaAnemoneFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SeaPickleFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SeagrassFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SpeleothemClusterFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SpringFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SulfurPoolSnapToSurfaceFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SulfurSpikeClusterFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SulfurSpikeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SulfurSpringTrailToSurfaceFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SwampFlowerDiscFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SwampSeagrassFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/SweetBerryBushFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/TaigaGrassFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/TallGrassDoublePlantFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/UnderwaterMagmaFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/VegetationFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/VegetationPatchFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/WarmOceanSeagrassFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/WaterlilyFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/WildflowersBedFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Multi/JunglePlantAggregateFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Multi/LegacySwampFoliageFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Multi/OverworldCaveCarverAggregateFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Multi/RoofedForestAggregateFeature.h"

void registerDecorationFeatures() {
    GenerateFeatureRegistry::registerFeature(std::make_shared<AzaleaRootSystemFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<BambooFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<BushFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CactusFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<ClayWithDripleavesPatchFeature>());
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
    GenerateFeatureRegistry::registerFeature(std::make_shared<GeodeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<HugeMushroomFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IcePatchFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IceSpikeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IcebergFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<JungleGrassFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<KelpFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LargeFernDoublePlantFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacyFlowerFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacyForestFoliageFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacyForestRockFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacySmallBrownMushroomsFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacySmallMushroomsFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MelonFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MesaFoliageFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MonsterRoomFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MossCeilingPatchFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MultifaceFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<PaleMossPatchFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<PlainsFlowerFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<PumpkinFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<ReedsFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SculkPatchFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SeaAnemoneFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SeaPickleFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SeagrassFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<OceanSeagrassFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<WarmOceanSeagrassFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SwampSeagrassFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SpeleothemClusterFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SpringFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SulfurPoolSnapToSurfaceFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SulfurSpikeClusterFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SulfurSpikeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SulfurSpringTrailToSurfaceFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SwampFlowerDiscFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SweetBerryBushFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<TaigaGrassFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<TallGrassDoublePlantFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<UnderwaterMagmaFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<VegetationFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<VegetationPatchFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<WaterlilyFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<WildflowersBedFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<JunglePlantAggregateFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacySwampFoliageFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<OverworldCaveCarverAggregateFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<RoofedForestAggregateFeature>());
}
