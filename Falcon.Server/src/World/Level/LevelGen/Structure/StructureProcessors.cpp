#include "World/Level/LevelGen/Structure/StructureProcessors.h"

#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"

#include <unordered_map>

namespace {
    using Test = StructureProcessors::Test;
    using Rule = StructureProcessors::Rule;

    // in registration order
    const std::unordered_map<std::string, std::vector<Rule>> &lists() {
        static const std::unordered_map<std::string, std::vector<Rule>> LISTS = {
                {"minecraft:cobblestone_building_low_decay",
                 {{Test::RandomBlockMatch, "minecraft:cobblestone", 0.1f, nullptr, "minecraft:mossy_cobblestone",
                   nullptr, 0}}},
                {"minecraft:cobblestone_building_decay",
                 {{Test::RandomBlockMatch, "minecraft:cobblestone", 0.2f, nullptr, "minecraft:mossy_cobblestone",
                   nullptr, 0}}},
                {"minecraft:cobblestone_building_high_decay",
                 {{Test::RandomBlockMatch, "minecraft:cobblestone", 0.7f, nullptr, "minecraft:mossy_cobblestone",
                   nullptr, 0}}},
                {"minecraft:abandoned_plains_decay",
                 {{Test::RandomBlockMatch, "minecraft:cobblestone", 0.8f, nullptr, "minecraft:mossy_cobblestone",
                   nullptr, 0},
                  {Test::BlockMatch, "minecraft:wooden_door", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:cobblestone", 0.07f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:mossy_cobblestone", 0.07f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:white_terracotta", 0.07f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.1f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_stairs", 0.1f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:stripped_oak_log", 0.02f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:glass_pane", 0.5f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:white_stained_glass_pane", 0.2f, nullptr, "minecraft:web",
                   nullptr, 0},
                  {Test::BlockMatch, "minecraft:glass_pane", 0.0f, nullptr, "minecraft:brown_stained_glass_pane",
                   nullptr, 0}}},
                {"minecraft:abandoned_savanna_decay",
                 {{Test::BlockMatch, "minecraft:acacia_door", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:white_terracotta", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:glass_pane", 0.5f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:white_stained_glass_pane", 0.2f, nullptr, "minecraft:web",
                   nullptr, 0},
                  {Test::BlockMatch, "minecraft:glass_pane", 0.0f, nullptr, "minecraft:brown_stained_glass_pane",
                   nullptr, 0}}},
                {"minecraft:abandoned_snowy_decay",
                 {{Test::BlockMatch, "minecraft:spruce_door", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_slab", 0.4f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:glass_pane", 0.5f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:white_stained_glass_pane", 0.2f, nullptr, "minecraft:web",
                   nullptr, 0},
                  {Test::BlockMatch, "minecraft:glass_pane", 0.0f, nullptr, "minecraft:brown_stained_glass_pane",
                   nullptr, 0}}},
                {"minecraft:abandoned_taiga_decay",
                 {{Test::RandomBlockMatch, "minecraft:cobblestone", 0.8f, nullptr, "minecraft:mossy_cobblestone",
                   nullptr, 0},
                  {Test::BlockMatch, "minecraft:spruce_door", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:campfire", 0.0f, nullptr, "minecraft:campfire", "extinguished", 1},
                  {Test::RandomBlockMatch, "minecraft:cobblestone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:glass_pane", 0.5f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:white_stained_glass_pane", 0.2f, nullptr, "minecraft:web",
                   nullptr, 0},
                  {Test::BlockMatch, "minecraft:glass_pane", 0.0f, nullptr, "minecraft:brown_stained_glass_pane",
                   nullptr, 0}}},
                {"minecraft:abandoned_desert_decay",
                 {{Test::BlockMatch, "minecraft:jungle_door", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.09f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:hardened_clay", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:smooth_sandstone_stairs", 0.08f, nullptr, "minecraft:web",
                   nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:smooth_sandstone_slab", 0.08f, nullptr, "minecraft:web", nullptr,
                   0}}},
                {"minecraft:plains_path_decay",
                 {{Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:water", "minecraft:oak_planks", nullptr,
                   0},
                  {Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:grass_block", "minecraft:grass_block",
                   nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:grass_block", 0.025f, nullptr, "minecraft:grass_block", nullptr,
                   0},
                  {Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:water", "minecraft:water", nullptr, 0},
                  {Test::BlockMatch, "minecraft:dirt", 0.0f, "minecraft:water", "minecraft:water", nullptr, 0}}},
                {"minecraft:desert_path_decay",
                 {{Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:grass_block", "minecraft:grass_block",
                   nullptr, 0}}},
                {"minecraft:savanna_path_decay",
                 {{Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:water", "minecraft:acacia_planks",
                   nullptr, 0},
                  {Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:grass_block", "minecraft:grass_block",
                   nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:grass_block", 0.05f, nullptr, "minecraft:grass_block", nullptr,
                   0},
                  {Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:water", "minecraft:water", nullptr, 0},
                  {Test::BlockMatch, "minecraft:dirt", 0.0f, "minecraft:water", "minecraft:water", nullptr, 0}}},
                {"minecraft:snowy_path_decay",
                 {{Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:water", "minecraft:spruce_planks",
                   nullptr, 0},
                  {Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:grass_block", "minecraft:grass_block",
                   nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:grass_block", 0.05f, nullptr, "minecraft:grass_block", nullptr,
                   0},
                  {Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:water", "minecraft:water", nullptr, 0},
                  {Test::BlockMatch, "minecraft:dirt", 0.0f, "minecraft:water", "minecraft:water", nullptr, 0}}},
                {"minecraft:taiga_path_decay",
                 {{Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:water", "minecraft:spruce_planks",
                   nullptr, 0},
                  {Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:grass_block", "minecraft:grass_block",
                   nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:grass_block", 0.05f, nullptr, "minecraft:grass_block", nullptr,
                   0},
                  {Test::BlockMatch, "minecraft:grass_block", 0.0f, "minecraft:water", "minecraft:water", nullptr, 0},
                  {Test::BlockMatch, "minecraft:dirt", 0.0f, "minecraft:water", "minecraft:water", nullptr, 0}}},
                {"minecraft:wheat_wheat_farm",
                 {{Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:wheat", "growth", 7}}},
                {"minecraft:wheat_potato_farm",
                 {{Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:potatoes", "growth", 7}}},
                {"minecraft:wheat_beet_farm",
                 {{Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:beetroot", "growth", 7}}},
                {"minecraft:potato_potato_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:potatoes", "growth", 7},
                  {Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:potatoes", "growth", 7}}},
                {"minecraft:potato_beet_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:potatoes", "growth", 7},
                  {Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:beetroot", "growth", 7}}},
                {"minecraft:potato_carrot_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:potatoes", "growth", 7}}},
                {"minecraft:beet_beet_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:beetroot", "growth", 7},
                  {Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:beetroot", "growth", 7}}},
                {"minecraft:beet_carrot_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:beetroot", "growth", 7}}},
                {"minecraft:carrot_carrot_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:carrots", "growth", 7}}},
                {"minecraft:abandoned_wheat_wheat_farm",
                 {{Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:wheat", "growth", 7},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.1f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0}}},
                {"minecraft:abandoned_wheat_potato_farm",
                 {{Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:potatoes", "growth", 7},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.1f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0}}},
                {"minecraft:abandoned_wheat_beet_farm",
                 {{Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:beetroot", "growth", 7},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0}}},
                {"minecraft:abandoned_potato_potato_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:potatoes", "growth", 7},
                  {Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:potatoes", "growth", 7},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0}}},
                {"minecraft:abandoned_potato_beet_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:potatoes", "growth", 7},
                  {Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:beetroot", "growth", 7},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0}}},
                {"minecraft:abandoned_potato_carrot_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:potatoes", "growth", 7},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0}}},
                {"minecraft:abandoned_beet_beet_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:beetroot", "growth", 7},
                  {Test::BlockMatch, "minecraft:carrots", 0.0f, nullptr, "minecraft:beetroot", "growth", 7},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0}}},
                {"minecraft:abandoned_beet_carrot_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:beetroot", "growth", 7},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0}}},
                {"minecraft:abandoned_carrot_carrot_farm",
                 {{Test::BlockMatch, "minecraft:wheat", 0.0f, nullptr, "minecraft:carrots", "growth", 7},
                  {Test::BlockMatch, "minecraft:torch", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::BlockMatch, "minecraft:lantern", 0.0f, nullptr, "minecraft:grass_block", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:sandstone", 0.08f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_log", 0.05f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_planks", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:acacia_stairs", 0.2f, nullptr, "minecraft:web", nullptr, 0},
                  {Test::RandomBlockMatch, "minecraft:oak_wood", 0.05f, nullptr, "minecraft:web", nullptr, 0}}},
        };
        return LISTS;
    }

    // no draw at or past the bounds
    bool chance(std::mt19937 &random, float probability) {
        if (probability <= 0.0f)
            return false;
        if (probability >= 1.0f)
            return true;

        return probability > FeatureRandom::nextFloat(random);
    }

    bool passes(Test test, const char *name, float probability, const BlockState &block, std::mt19937 &random) {
        switch (test) {
            case Test::AlwaysTrue:
                return true;
            case Test::BlockMatch:
                return block.mName == name;
            case Test::RandomBlockMatch:
                // name first: another block draws nothing
                return block.mName == name && chance(random, probability);
        }
        return false;
    }

    BlockState output(const Rule &rule) {
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(rule.mOutput);
        Tag states = defaults != nullptr ? *defaults : Tag::ofCompound();
        if (rule.mOutputState != nullptr) {
            if (states.contains(rule.mOutputState, Tag::Type::Byte))
                states.putByte(rule.mOutputState, (int8_t) rule.mOutputValue);
            else
                states.putInt(rule.mOutputState, rule.mOutputValue);
        }
        return BlockState(rule.mOutput, states);
    }
}

const std::vector<StructureProcessors::Rule> &StructureProcessors::get(const std::string &name) {
    static const std::vector<Rule> NONE;
    const auto found = lists().find(name);
    return found != lists().end() ? found->second : NONE;
}

BlockState StructureProcessors::apply(const std::vector<const Rule *> &rules, const BlockState &block,
                                      const BlockState &world, std::mt19937 &random) {
    BlockState current = block;
    for (const Rule *rule: rules) {
        if (!passes(rule->mInput, rule->mInputBlock, rule->mProbability, current, random))
            continue;
        if (rule->mLocationBlock != nullptr && world.mName != rule->mLocationBlock)
            continue;

        current = output(*rule);
    }
    return current;
}
