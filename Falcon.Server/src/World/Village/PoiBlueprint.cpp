#include "World/Village/PoiBlueprint.h"

#include <unordered_map>

namespace {
    const char *BECOME_UNSKILLED = "minecraft:become_unskilled";
    const float JOB_SITE_RADIUS = 2.0f;

    PoiBlueprint jobSite(const char *name, const char *initEvent, const char *soundEvent, uint64_t weight) {
        return {name, initEvent, BECOME_UNSKILLED, soundEvent, PoiType::JobSite, JOB_SITE_RADIUS, 1, weight, false};
    }

    // Registered by the vanilla village manager constructor, keyed by block type. Beds and bells carry no events
    // and the "undefined" sound.
    const std::unordered_map<std::string, PoiBlueprint> &blueprints() {
        static const std::unordered_map<std::string, PoiBlueprint> table = {
                {"minecraft:bed", {"villager", "", "", "undefined", PoiType::Bed, 0.75f, 1, 1, true}},
                {"minecraft:bell", {"villager", "", "", "undefined", PoiType::MeetingArea, 7.0f, 20, 1, false}},
                {"minecraft:composter",
                 jobSite("farmer", "minecraft:become_farmer", "block.composter.fill", 1)},
                {"minecraft:lectern", jobSite("librarian", "minecraft:become_librarian", "item.book.put", 8)},
                {"minecraft:grindstone",
                 jobSite("weaponsmith", "minecraft:become_weaponsmith", "block.grindstone.use", 5)},
                {"minecraft:blast_furnace",
                 jobSite("armorer", "minecraft:become_armorer", "block.blastfurnace.fire_crackle", 5)},
                {"minecraft:lit_blast_furnace",
                 jobSite("armorer", "minecraft:become_armorer", "block.blastfurnace.fire_crackle", 5)},
                {"minecraft:smoker", jobSite("butcher", "minecraft:become_butcher", "block.smoker.smoke", 5)},
                {"minecraft:lit_smoker", jobSite("butcher", "minecraft:become_butcher", "block.smoker.smoke", 5)},
                {"minecraft:fletching_table",
                 jobSite("fletcher", "minecraft:become_fletcher", "block.fletching_table.use", 5)},
                {"minecraft:cartography_table",
                 jobSite("cartographer", "minecraft:become_cartographer", "block.cartography_table.use", 6)},
                {"minecraft:brewing_stand", jobSite("cleric", "minecraft:become_cleric", "potion.brewed", 8)},
                {"minecraft:smithing_table",
                 jobSite("toolsmith", "minecraft:become_toolsmith", "block.smithing_table.use", 5)},
                {"minecraft:cauldron",
                 jobSite("leatherworker", "minecraft:become_leatherworker", "bucket.fill.water", 5)},
                {"minecraft:barrel", jobSite("fisherman", "minecraft:become_fisherman", "block.barrel.open", 3)},
                // The vanilla shepherd event name is misspelled.
                {"minecraft:loom", jobSite("shepherd", "minecraft:become_sheperd", "block.loom.use", 3)},
                {"minecraft:stonecutter_block",
                 jobSite("mason", "minecraft:become_mason", "block.stonecutter.use", 3)},
        };
        return table;
    }
}

const PoiBlueprint *PoiBlueprints::find(const std::string &identifier) {
    const auto &table = blueprints();
    const auto found = table.find(identifier);
    return found == table.end() ? nullptr : &found->second;
}
