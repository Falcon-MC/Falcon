#include "Level/Generator/Feature/GenerateFeatureRegistry.h"

#include "Level/Generator/Overworld/Feature/Decoration/AzaleaRootSystemFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/BambooFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/BushFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/CactusFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/ClayWithDripleavesPatchFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/CoralCrustFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/CoralHangFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/CoralTreeFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/DeadBushFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/DoublePlantFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/DryGrassDiscFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/EyeblossomFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/FireflyBushClusterFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/FireflyBushWaterClusterFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/FlowerBedFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/FlowerForestFoliageFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/ForestFlowerFoliageFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/GeodeFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/HugeMushroomFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/IcePatchFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/IceSpikeFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/IcebergFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/JungleGrassFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/KelpFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/LargeFernDoublePlantFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/LegacyFlowerFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/LegacyForestFoliageFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/LegacyForestRockFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/LegacySmallBrownMushroomsFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/LegacySmallMushroomsFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/MelonFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/MesaFoliageFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/MonsterRoomFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/MossCeilingPatchFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/MultifaceFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/OceanSeagrassFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/PaleMossPatchFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/PlainsFlowerFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/PumpkinFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/ReedsFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SculkPatchFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SeaAnemoneFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SeaPickleFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SeagrassFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SpeleothemClusterFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SpringFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SulfurPoolSnapToSurfaceFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SulfurSpikeClusterFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SulfurSpikeFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SulfurSpringTrailToSurfaceFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/IceCavesSnowLayerFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/IceCavesSurfaceFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/IceCrystalScatterFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/IcicleClusterFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/LargeIcicleFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SwampFlowerDiscFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SwampSeagrassFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/SweetBerryBushFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/TaigaGrassFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/TallGrassDoublePlantFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/UnderwaterMagmaFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/VegetationFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/VegetationPatchFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/WarmOceanSeagrassFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/WaterlilyFeature.h"
#include "Level/Generator/Overworld/Feature/Decoration/WildflowersBedFeature.h"
#include "Level/Generator/Overworld/Feature/Multi/JunglePlantAggregateFeature.h"
#include "Level/Generator/Overworld/Feature/Multi/LegacySwampFoliageFeature.h"
#include "Level/Generator/Overworld/Feature/Multi/OverworldCaveCarverAggregateFeature.h"
#include "Level/Generator/Overworld/Feature/Multi/RoofedForestAggregateFeature.h"

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
    GenerateFeatureRegistry::registerFeature(std::make_shared<IceCavesSurfaceFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IceCrystalScatterFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IcicleClusterFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LargeIcicleFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IceCavesSnowLayerFeature>());
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
