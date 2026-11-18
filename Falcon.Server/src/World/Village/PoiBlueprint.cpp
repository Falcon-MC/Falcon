#include "World/Village/PoiBlueprint.h"

#include <unordered_map>

namespace {

    const float JOB_SITE_RADIUS = 2.0f;
    const float BED_RADIUS = 0.75f;
    const float BELL_RADIUS = 7.0f;

    PoiBlueprint jobSite(const char *name, const char *initEvent, uint64_t weight, const char *sound) {
        return {name,  initEvent, PoiBlueprints::BECOME_UNSKILLED, PoiType::JobSite, 1, weight, JOB_SITE_RADIUS,
                false, sound};
    }

    // the game's hard-coded table from the village manager constructor, keyed by block type
    const std::unordered_map<std::string, PoiBlueprint> &blueprints() {
        static const std::unordered_map<std::string, PoiBlueprint> table = {
                {"minecraft:bed", {"villager", "", "", PoiType::Bed, 1, 1, BED_RADIUS, true, ""}},
                {"minecraft:bell", {"villager", "", "", PoiType::MeetingArea, 20, 1, BELL_RADIUS, false, ""}},
                {"minecraft:composter", jobSite("farmer", "minecraft:become_farmer", 1, "block.composter.fill")},
                {"minecraft:lectern", jobSite("librarian", "minecraft:become_librarian", 8, "item.book.put")},
                {"minecraft:grindstone",
                 jobSite("weaponsmith", "minecraft:become_weaponsmith", 5, "block.grindstone.use")},
                {"minecraft:blast_furnace",
                 jobSite("armorer", "minecraft:become_armorer", 5, "block.blastfurnace.fire_crackle")},
                {"minecraft:lit_blast_furnace",
                 jobSite("armorer", "minecraft:become_armorer", 5, "block.blastfurnace.fire_crackle")},
                {"minecraft:smoker", jobSite("butcher", "minecraft:become_butcher", 5, "block.smoker.smoke")},
                {"minecraft:lit_smoker", jobSite("butcher", "minecraft:become_butcher", 5, "block.smoker.smoke")},
                {"minecraft:fletching_table",
                 jobSite("fletcher", "minecraft:become_fletcher", 5, "block.fletching_table.use")},
                {"minecraft:cartography_table",
                 jobSite("cartographer", "minecraft:become_cartographer", 6, "block.cartography_table.use")},
                {"minecraft:brewing_stand", jobSite("cleric", "minecraft:become_cleric", 8, "potion.brewed")},
                {"minecraft:smithing_table",
                 jobSite("toolsmith", "minecraft:become_toolsmith", 5, "block.smithing_table.use")},
                {"minecraft:cauldron",
                 jobSite("leatherworker", "minecraft:become_leatherworker", 5, "bucket.fill.water")},
                {"minecraft:barrel", jobSite("fisherman", "minecraft:become_fisherman", 3, "block.barrel.open")},
                // misspelled in the game
                {"minecraft:loom", jobSite("shepherd", "minecraft:become_sheperd", 3, "block.loom.use")},
                {"minecraft:stonecutter_block", jobSite("mason", "minecraft:become_mason", 3, "block.stonecutter.use")},
        };
        return table;
    }
}

const PoiBlueprint *PoiBlueprints::find(const std::string &identifier) {
    const auto &table = blueprints();
    const auto found = table.find(identifier);
    return found == table.end() ? nullptr : &found->second;
}
