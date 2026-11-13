#include "World/Item/ExplorationMaps.h"

#include "World/Actor/Actor.h"
#include "World/Item/MapItem.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/World.h"
#include "Network/Handler/MapHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

namespace {
    using Type = MapDecorationType;

    // the abandoned camps search a biome rather than a structure
    const ExplorationMaps::Entry ENTRIES[] = {
            {3, Type::Monument, "monument", "monument"},
            {4, Type::Mansion, "", "mansion"},
            {5, Type::XWhite, "", "buriedtreasure"},
            {7, Type::VillageSnowy, "", "village_snowy"},
            {8, Type::VillageTaiga, "", "village_taiga"},
            {9, Type::VillagePlains, "", "village_plains"},
            {10, Type::VillageSavanna, "", "village_savanna"},
            {11, Type::VillageDesert, "", "village_desert"},
            {12, Type::JungleTemple, "", "jungle_temple"},
            {13, Type::WitchHut, "", "swamp_hut"},
            {14, Type::TrialChambers, "trial_chambers", "trial_chambers"},
            {15, (Type) 25, "", "abandoned_camp_bamboo_jungle"},
            {16, (Type) 25, "", "abandoned_camp_birch_forest"},
            {17, (Type) 25, "", "abandoned_camp_cherry_grove"},
            {18, (Type) 25, "", "abandoned_camp_dappled_forest"},
            {19, (Type) 25, "", "abandoned_camp_extreme_hills_plus_trees"},
            {20, (Type) 25, "", "abandoned_camp_flower_forest"},
            {21, (Type) 25, "", "abandoned_camp_pale_garden"},
            {22, (Type) 25, "", "abandoned_camp_swampland"},
            {23, (Type) 26, "", "buried_ancient_city"},
            {24, (Type) 27, "", "buried_mineshaft"},
            {25, (Type) 28, "", "desert_pyramid"},
            {26, (Type) 29, "", "warm_ocean_ruins"},
    };

    const int8_t EXPLORATION_SCALE = 2;
}

const ExplorationMaps::Entry *ExplorationMaps::findByAux(int32_t aux) {
    for (const Entry &entry: ENTRIES) {
        if (entry.mAux == aux)
            return &entry;
    }
    return nullptr;
}

const ExplorationMaps::Entry *ExplorationMaps::findByDestination(const std::string &destination) {
    for (const Entry &entry: ENTRIES) {
        if (destination == entry.mDestination)
            return &entry;
    }
    return nullptr;
}

void ExplorationMaps::create(ServerNetworkHandler &owner, ItemStack &item, const Actor *searcher, const Entry &entry) {
    World &world = searcher != nullptr ? owner.getWorldFor(*searcher) : owner.getWorldOf(nullptr);
    const DimensionType dimension = searcher != nullptr ? searcher->getDimension() : DimensionType::Overworld;
    Vector3f position;
    if (searcher != nullptr) {
        const Vector3f &actor = searcher->getPosition();
        position = Vector3f(std::floor(actor.x), std::floor(actor.y), std::floor(actor.z));
    }

    // only the overworld is searched; without a structure the map centres on the searcher and has no marker
    bool found = false;
    const StructureFeature *feature = entry.mStructure[0] == '\0' ? nullptr : StructureFeature::get(entry.mStructure);
    Level &level = world.getLevel(dimension);
    if (dimension == DimensionType::Overworld && feature != nullptr && level.shareGenerator() != nullptr) {
        StructureChunkPos start;
        if (feature->findNearest(level.getGenerator(), (int32_t) position.x, (int32_t) position.z, start)) {
            position = Vector3f((float) (start.x * 16 + 8), position.y, (float) (start.z * 16 + 8));
            found = true;
        }
    }

    if (entry.mAux > 0)
        item.mDamage = entry.mAux;

    Level &mapLevel = world.getOverworld();
    MapItemSavedData &data = mapLevel.getMaps().createMapSavedData([&owner]() { return owner.allocateActorUniqueId(); },
                                                                   position, Dimension::toId(dimension),
                                                                   EXPLORATION_SCALE);
    MapItem::setMapId(item, data);
    data.setUnlimitedTracking(true);

    if (found) {
        const MapItemTrackedActorKey key = MapItemTrackedActorKey::block(
                Vector3i((int32_t) position.x, (int32_t) position.y, (int32_t) position.z));
        MapItemTrackingInfo &tracking = data.addTracked(key, data.getDimension(), entry.mDecoration);
        MapItem::updateTrackedDecoration(owner, world, data, tracking);
    }

    data.setPreviewIncomplete(true);
    MapHandler::broadcastCreation(owner, world, data);
}
