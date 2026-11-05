#include "World/Village/PoiBlueprint.h"

#include <unordered_map>

namespace {
    const char *BECOME_UNSKILLED = "minecraft:become_unskilled";

    PoiBlueprint jobSite(const char *name, const char *initEvent, uint64_t weight) {
        return {name, initEvent, BECOME_UNSKILLED, PoiType::JobSite, 1, weight};
    }

    // Registered by the vanilla village manager constructor, keyed by block type. Beds and bells carry no events.
    const std::unordered_map<std::string, PoiBlueprint> &blueprints() {
        static const std::unordered_map<std::string, PoiBlueprint> table = {
                {"minecraft:bed", {"villager", "", "", PoiType::Bed, 1, 1}},
                {"minecraft:bell", {"villager", "", "", PoiType::MeetingArea, 20, 1}},
                {"minecraft:composter", jobSite("farmer", "minecraft:become_farmer", 1)},
                {"minecraft:lectern", jobSite("librarian", "minecraft:become_librarian", 8)},
                {"minecraft:grindstone", jobSite("weaponsmith", "minecraft:become_weaponsmith", 5)},
                {"minecraft:blast_furnace", jobSite("armorer", "minecraft:become_armorer", 5)},
                {"minecraft:lit_blast_furnace", jobSite("armorer", "minecraft:become_armorer", 5)},
                {"minecraft:smoker", jobSite("butcher", "minecraft:become_butcher", 5)},
                {"minecraft:lit_smoker", jobSite("butcher", "minecraft:become_butcher", 5)},
                {"minecraft:fletching_table", jobSite("fletcher", "minecraft:become_fletcher", 5)},
                {"minecraft:cartography_table", jobSite("cartographer", "minecraft:become_cartographer", 6)},
                {"minecraft:brewing_stand", jobSite("cleric", "minecraft:become_cleric", 8)},
                {"minecraft:smithing_table", jobSite("toolsmith", "minecraft:become_toolsmith", 5)},
                {"minecraft:cauldron", jobSite("leatherworker", "minecraft:become_leatherworker", 5)},
                {"minecraft:barrel", jobSite("fisherman", "minecraft:become_fisherman", 3)},
                // The vanilla shepherd event name is misspelled.
                {"minecraft:loom", jobSite("shepherd", "minecraft:become_sheperd", 3)},
                {"minecraft:stonecutter_block", jobSite("mason", "minecraft:become_mason", 3)},
        };
        return table;
    }
}

const PoiBlueprint *PoiBlueprints::find(const std::string &identifier) {
    const auto &table = blueprints();
    const auto found = table.find(identifier);
    return found == table.end() ? nullptr : &found->second;
}
