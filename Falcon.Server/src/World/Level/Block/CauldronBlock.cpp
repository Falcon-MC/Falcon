#include "World/Level/Block/CauldronBlock.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Block/Actor/CauldronBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Level.h"

#include <algorithm>

FALCON_REGISTER_BLOCK(CauldronBlock, 200);

namespace {
    // LevelEvent ids of a cauldron use
    const int32_t EVENT_DYE_ARMOR = 3502;
    const int32_t EVENT_CLEAN_ARMOR = 3503;
    const int32_t EVENT_ADD_DYE = 3508;
    const char *const TAG_CUSTOM_COLOR = "customColor";

    // dyeable is the leather tier armor; the horse armor tag, wolf armor and pack items with minecraft:dyeable also
    // take a color
    bool takesColor(const std::string &identifier) {
        if (identifier == "minecraft:wolf_armor")
            return true;
        if (const CustomItemDefinition *custom = CustomContentRegistry::getInstance().findItem(identifier))
            return custom->mHasDyeable;
        for (const std::string &tag: CraftingRecipeTable::getItemTags(identifier)) {
            if (tag == "minecraft:leather_tier" || tag == "minecraft:horse_armor")
                return true;
        }
        return false;
    }

    // the event sits on the water surface and carries the water color
    void broadcastCauldronEvent(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const CauldronBlockActor &cauldron, int32_t eventId) {
        const int fill = CauldronBlock(level.getBlockState(position.x, position.y, position.z)).getFillLevel();
        const Vector3f at((float) position.x + 0.5f, (float) position.y + 0.375f + (float) (fill * 2) * 0.0625f,
                          (float) position.z + 0.5f);
        owner.broadcastLevelEvent(level, eventId, at, (int32_t) cauldron.getEventColor());
    }

    void putInHand(ServerPlayer &player, const ItemStack &item) {
        PlayerInventory &inventory = player.getInventory();
        inventory.setItemInHand(item.mCount > 0 ? item : ItemStack::air());
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    }
}

bool CauldronBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:cauldron";
}

int CauldronBlock::getFillLevel() const {
    return std::clamp(getStates().getInt("fill_level", 0), 0, 6);
}

CauldronBlock::Liquid CauldronBlock::getLiquid() const {
    if (getFillLevel() == 0)
        return Liquid::Empty;
    const std::string liquid = getStates().getString("cauldron_liquid", "water");
    if (liquid == "lava")
        return Liquid::Lava;
    return liquid == "powder_snow" ? Liquid::PowderSnow : Liquid::Water;
}

bool CauldronBlock::isFull() const {
    return getFillLevel() == 6;
}

bool CauldronBlock::canFill(Liquid liquid) const {
    if (liquid == Liquid::Empty)
        return false;
    if (getFillLevel() == 6)
        return false;
    return getFillLevel() == 0 || getLiquid() == liquid;
}

BlockState CauldronBlock::withFillLevel(Liquid liquid, int fillLevel) const {
    return makeState(liquid, fillLevel);
}

BlockState CauldronBlock::makeState(Liquid liquid, int fillLevel) {
    Tag states = Tag::ofCompound();
    states.putString("cauldron_liquid", liquid == Liquid::Lava         ? "lava"
                                        : liquid == Liquid::PowderSnow ? "powder_snow"
                                                                       : "water");
    states.putInt("fill_level", std::clamp(fillLevel, 0, 6));
    return BlockState("minecraft:cauldron", states);
}

// CauldronBlock::getComparatorSignal: half the fill level, rounded up
int CauldronBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    return (std::clamp(state.mStates.getInt("fill_level", 0), 0, 6) + 1) / 2;
}

// its dye branches: water without a potion only
bool CauldronBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                               const BlockState &state) const {
    const CauldronBlock cauldron(state);
    const int fill = cauldron.getFillLevel();
    if (cauldron.getLiquid() != Liquid::Water)
        return false;
    const ItemStack held = player.getInventory().getItemInHand();
    if (held.isAir() || held.mDefinition == nullptr)
        return false;

    Level &level = owner.getLevelFor(player);
    CauldronBlockActor &actor = level.getBlockActors().getOrCreate<CauldronBlockActor>(position);
    if (actor.hasPotion())
        return false;

    const std::string &identifier = held.mDefinition->getIdentifier();
    if (takesColor(identifier)) {
        ItemStack colored = held;
        if (!actor.hasColor()) {
            // plain water washes the color off, and only an item that has one
            if (!colored.mTag.isCompound() || !colored.mTag.contains(TAG_CUSTOM_COLOR))
                return false;
            colored.mTag.remove(TAG_CUSTOM_COLOR);
            level.setBlock(position, withFillLevel(Liquid::Water, fill - 1), true);
            broadcastCauldronEvent(owner, level, position, actor, EVENT_CLEAN_ARMOR);
        } else {
            if (!colored.mTag.isCompound())
                colored.mTag = Tag::ofCompound();
            colored.mTag.putInt(TAG_CUSTOM_COLOR, (int32_t) (0xFF000000u | ((uint32_t) actor.getColor() & 0xFFFFFFu)));
            level.setBlock(position, withFillLevel(Liquid::Water, fill - 1), true);
            broadcastCauldronEvent(owner, level, position, actor, EVENT_DYE_ARMOR);
        }
        putInHand(player, colored);
        if (fill <= 1) {
            actor.clearContents();
            BlockActionHandler::broadcastBlockActorData(owner, level, actor);
        }
        return true;
    }

    const int32_t dyeColor = CauldronBlockActor::dyeColorOf(identifier);
    if (dyeColor < 0)
        return false;
    if (!player.isCreative()) {
        ItemStack remaining = held;
        remaining.mCount -= 1;
        putInHand(player, remaining);
    }
    actor.addDye(dyeColor);
    BlockActionHandler::broadcastBlockActorData(owner, level, actor);
    // the game sets the level one lower then back so the clients redraw the water
    level.setBlock(position, withFillLevel(Liquid::Water, fill - 1), true);
    level.setBlock(position, withFillLevel(Liquid::Water, fill), true);
    broadcastCauldronEvent(owner, level, position, actor, EVENT_ADD_DYE);
    return true;
}

const char *CauldronBlock::getBlockActorId() const {
    return CauldronBlockActor::BLOCK_ACTOR_ID;
}
