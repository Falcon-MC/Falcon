#include "World/Level/Block/ComposterBlock.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemNetworkIdTable.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace {
    const char *const FILL_LEVEL = "composter_fill_level";
    const int32_t FULL_LEVEL = 7;
    const int32_t READY_LEVEL = 8;
    // a full composter turns ready this many ticks later
    const int64_t READY_DELAY = 20;
    const char *const EMPTY_SOUND = "block.composter.empty";
    const char *const FILL_SOUND = "block.composter.fill";
    const char *const FILL_SUCCESS_SOUND = "block.composter.fill_success";
    const char *const READY_SOUND = "block.composter.ready";
    const char *const BONE_MEAL = "minecraft:bone_meal";
    const char *const COMPOSTABLE_COMPONENT = "minecraft:compostable";
    // the particles of a filled composter, as bone meal's
    const int32_t FILL_EVENT = 2005;
    const float FILL_EVENT_HEIGHT = 0.75f;

    // the game's own table, keyed by the block an item places or else the item itself
    const std::unordered_map<std::string, int32_t> &compostChances() {
        static const std::unordered_map<std::string, int32_t> CHANCES = [] {
            const std::pair<const char *, int32_t> entries[] = {
            {"oak_leaves", 30}, {"spruce_leaves", 30}, {"birch_leaves", 30}, {"jungle_leaves", 30},
            {"acacia_leaves", 30}, {"dark_oak_leaves", 30}, {"oak_sapling", 30}, {"spruce_sapling", 30},
            {"birch_sapling", 30}, {"jungle_sapling", 30}, {"acacia_sapling", 30}, {"dark_oak_sapling", 30},
            {"seagrass", 30}, {"grass_block", 30}, {"kelp", 30}, {"dried_kelp", 30}, {"beetroot_seeds", 30},
            {"melon_seeds", 30}, {"pumpkin_seeds", 30}, {"wheat_seeds", 30}, {"sweet_berries", 30},
            {"azalea_leaves", 30}, {"glow_berries", 30}, {"moss_carpet", 30}, {"hanging_roots", 30},
            {"mangrove_leaves", 30}, {"mangrove_roots", 30}, {"mangrove_propagule", 30}, {"cherry_sapling", 30},
            {"cherry_leaves", 30}, {"pitcher_pod", 30}, {"torchflower_seeds", 30}, {"pale_oak_leaves", 30},
            {"orange_poplar_leaves", 30}, {"red_poplar_leaves", 30}, {"yellow_poplar_leaves", 30},
            {"pale_moss_carpet", 30}, {"pale_oak_sapling", 30}, {"poplar_sapling", 30}, {"wildflowers", 30},
            {"firefly_bush", 30}, {"bush", 30}, {"leaf_litter", 30}, {"cactus_flower", 30}, {"short_dry_grass", 30},
            {"tall_dry_grass", 30},
            {"dried_kelp_block", 50}, {"short_grass", 50}, {"fern", 50}, {"cactus", 50}, {"vine", 50},
            {"weeping_vines", 50}, {"twisting_vines", 50}, {"melon_slice", 50}, {"sugar_cane", 50},
            {"nether_sprouts", 50}, {"azalea_leaves_flowered", 50}, {"glow_lichen", 50}, {"pale_hanging_moss", 50},
            {"sea_pickle", 65}, {"waterlily", 65}, {"pumpkin", 65}, {"carved_pumpkin", 65}, {"melon_block", 65},
            {"red_mushroom", 65}, {"brown_mushroom", 65}, {"crimson_fungus", 65}, {"warped_fungus", 65},
            {"nether_wart", 65}, {"cocoa_beans", 65}, {"poppy", 65}, {"blue_orchid", 65}, {"allium", 65},
            {"azure_bluet", 65}, {"red_tulip", 65}, {"orange_tulip", 65}, {"white_tulip", 65}, {"pink_tulip", 65},
            {"oxeye_daisy", 65}, {"cornflower", 65}, {"lily_of_the_valley", 65}, {"dandelion", 65},
            {"crimson_roots", 65}, {"warped_roots", 65}, {"sunflower", 65}, {"lilac", 65}, {"tall_grass", 65},
            {"large_fern", 65}, {"rose_bush", 65}, {"peony", 65}, {"carrot", 65}, {"potato", 65}, {"wheat", 65},
            {"beetroot", 65}, {"apple", 65}, {"wither_rose", 65}, {"shroomlight", 65}, {"mushroom_stem", 65},
            {"azalea", 65}, {"moss_block", 65}, {"spore_blossom", 65}, {"small_dripleaf_block", 65},
            {"big_dripleaf", 65}, {"pink_petals", 65}, {"pale_moss_block", 65}, {"open_eyeblossom", 65},
            {"closed_eyeblossom", 65},
            {"hay_block", 85}, {"brown_mushroom_block", 85}, {"red_mushroom_block", 85}, {"nether_wart_block", 85},
            {"warped_wart_block", 85}, {"bread", 85}, {"baked_potato", 85}, {"cookie", 85}, {"flowering_azalea", 85},
            {"pitcher_plant", 85}, {"torchflower", 85}, {"straw_bed", 85},
            {"cake", 100}, {"pumpkin_pie", 100}};
            std::unordered_map<std::string, int32_t> chances;
            for (const auto &entry: entries)
                chances.emplace(std::string("minecraft:") + entry.first, entry.second);
            return chances;
        }();
        return CHANCES;
    }

    int32_t numberIn(const Tag &value) {
        switch (value.getType()) {
            case Tag::Type::Byte:
                return value.asByte();
            case Tag::Type::Short:
                return value.asShort();
            case Tag::Type::Int:
                return value.asInt();
            case Tag::Type::Float:
                return (int32_t) value.asFloat();
            default:
                return 0;
        }
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    // a new state: the copied one would keep the hash of its old level
    BlockState withLevel(const BlockState &state, int32_t level) {
        Tag states = state.mStates;
        states.putInt(FILL_LEVEL, level);
        return BlockState(state.mName, states);
    }
}

FALCON_REGISTER_BLOCK(ComposterBlock, 100);

bool ComposterBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::COMPOSTER;
}

// ComposterBlock::getComparatorSignal: the fill level as is
int ComposterBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    return std::clamp(state.mStates.getInt(FILL_LEVEL, 0), 0, READY_LEVEL);
}

int32_t ComposterBlock::compostChance(const ItemStack &item) {
    if (item.isAir())
        return 0;

    // a minecraft:compostable component comes before the table
    const std::string &identifier = item.mDefinition->getIdentifier();
    if (const CustomItemDefinition *custom = CustomContentRegistry::getInstance().findItem(identifier))
        if (custom->mHasCompostable)
            return custom->mCompostingChance;
    if (const ItemNetworkIdEntry *entry = ItemNetworkIdTable::find(identifier)) {
        const Tag *components = entry->mComponents.get("components");
        const Tag *compostable = components == nullptr ? nullptr : components->get(COMPOSTABLE_COMPONENT);
        const Tag *chance = compostable == nullptr ? nullptr : compostable->get("composting_chance");
        if (chance != nullptr)
            return numberIn(*chance);
    }

    const std::unordered_map<std::string, int32_t> &chances = compostChances();
    if (item.mBlockDefinition != nullptr) {
        const auto block = chances.find(item.mBlockDefinition->getIdentifier());
        if (block != chances.end())
            return block->second;
    }
    const auto found = chances.find(identifier);
    return found == chances.end() ? 0 : found->second;
}

int32_t ComposterBlock::addItems(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const ItemStack &item, int32_t count) {
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    if (state == nullptr || !matches(state->mName) || item.isAir() || count > item.mCount)
        return 0;

    const int32_t chance = compostChance(item);
    const int32_t fill = state->mStates.getInt(FILL_LEVEL, 0);
    if (chance <= 0 || fill >= FULL_LEVEL)
        return 0;

    // one item after another until the composter is full; an empty one always takes its first layer
    int32_t used = 0;
    int32_t next = fill;
    while (used < count) {
        ++used;
        if (next != 0 && RandomTickSystem::nextInt(100) > chance)
            continue;
        if (++next == FULL_LEVEL)
            break;
    }

    if (next == fill) {
        owner.playLevelSound(level, FILL_SOUND, centerOf(position));
        return used;
    }

    level.setBlock(position, withLevel(*state, next), true);
    if (next == FULL_LEVEL)
        level.scheduleBlockUpdate(position, READY_DELAY);
    owner.playLevelSound(level, FILL_SUCCESS_SOUND, centerOf(position));
    return used;
}

bool ComposterBlock::addItem(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const ItemStack &item) {
    return addItems(owner, level, position, item, 1) > 0;
}

bool ComposterBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    const int32_t fill = state.mStates.getInt(FILL_LEVEL, 0);
    if (fill == READY_LEVEL) {
        const Vector3f top((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
        owner.dropItem(level, top, owner.createItemStack(BONE_MEAL, 1), ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
        takeOutput(owner, level, position);
        return true;
    }
    if (fill >= FULL_LEVEL)
        return false;

    const ItemStack held = player.getInventory().getItemInHand();
    if (!addItem(owner, level, position, held))
        return false;

    owner.broadcastLevelEvent(level, FILL_EVENT,
                              Vector3f((float) position.x, (float) position.y + FILL_EVENT_HEIGHT, (float) position.z),
                              0);
    player.consumeOneHeldItem();
    return true;
}

void ComposterBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    if (state.mStates.getInt(FILL_LEVEL, 0) != FULL_LEVEL)
        return;

    level.setBlock(position, withLevel(state, READY_LEVEL), true);
    owner.playLevelSound(level, READY_SOUND, centerOf(position));
}

bool ComposterBlock::takeOutput(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    if (state == nullptr || !matches(state->mName) || state->mStates.getInt(FILL_LEVEL, 0) != READY_LEVEL)
        return false;

    level.setBlock(position, withLevel(*state, 0), true);
    owner.playLevelSound(level, EMPTY_SOUND, centerOf(position));
    return true;
}
