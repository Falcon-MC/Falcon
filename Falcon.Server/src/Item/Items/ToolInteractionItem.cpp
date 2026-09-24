#include "Item/Items/ToolInteractionItem.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/Systems/CopperSystem.h"
#include "Item/ItemClassRegistry.h"
#include "Item/ItemData.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <string>

FALCON_REGISTER_ITEM(ToolInteractionItem, 120);

namespace {
    const std::string PREFIX = "minecraft:";
    const std::string STRIPPED = "minecraft:stripped_";
    const char *const STRIPPABLE_SUFFIXES[] = {"_log", "_wood", "_stem", "_hyphae"};

    enum class Effect {
        Sound,
        WaxOff,
        Scrape
    };

    struct Transformation {
        std::string mIdentifier;
        Effect mEffect = Effect::Sound;
        bool mNeedsAirAbove = false;
        const char *mDrop = nullptr;
    };

    bool startsWith(const std::string &value, const std::string &prefix) {
        return value.compare(0, prefix.size(), prefix) == 0;
    }

    bool endsWith(const std::string &value, const char *suffix) {
        const std::string tail(suffix);
        return value.size() >= tail.size() && value.compare(value.size() - tail.size(), tail.size(), tail) == 0;
    }

    bool exists(const std::string &identifier) {
        return BlockDataTable::find(identifier.c_str()) != nullptr;
    }

    std::string strippedOf(const std::string &identifier) {
        if (startsWith(identifier, STRIPPED))
            return std::string();

        bool strippable = identifier == "minecraft:bamboo_block";
        for (const char *suffix: STRIPPABLE_SUFFIXES) {
            if (endsWith(identifier, suffix))
                strippable = true;
        }

        if (!strippable)
            return std::string();

        const std::string candidate = STRIPPED + identifier.substr(PREFIX.size());
        return exists(candidate) ? candidate : std::string();
    }

    bool transformationFor(ToolType tool, const std::string &identifier, Transformation &out) {
        if (tool == ToolType::Hoe) {
            out.mNeedsAirAbove = true;
            if (identifier == "minecraft:dirt" || identifier == "minecraft:coarse_dirt"
                || identifier == "minecraft:grass_block" || identifier == "minecraft:grass_path") {
                out.mIdentifier = "minecraft:farmland";
                return true;
            }
            if (identifier == "minecraft:dirt_with_roots") {
                out.mIdentifier = "minecraft:dirt";
                out.mDrop = "minecraft:hanging_roots";
                return true;
            }
            return false;
        }

        if (tool == ToolType::Shovel) {
            out.mNeedsAirAbove = true;
            if (identifier == "minecraft:grass_block" || identifier == "minecraft:dirt"
                || identifier == "minecraft:coarse_dirt" || identifier == "minecraft:podzol"
                || identifier == "minecraft:mycelium" || identifier == "minecraft:dirt_with_roots") {
                out.mIdentifier = "minecraft:grass_path";
                return true;
            }
            return false;
        }

        if (tool == ToolType::Axe) {
            out.mIdentifier = strippedOf(identifier);
            if (!out.mIdentifier.empty())
                return true;

            out.mIdentifier = CopperSystem::withoutWaxOf(identifier);
            if (!out.mIdentifier.empty()) {
                out.mEffect = Effect::WaxOff;
                return true;
            }

            out.mIdentifier = CopperSystem::scrapedOf(identifier);
            if (!out.mIdentifier.empty()) {
                out.mEffect = Effect::Scrape;
                return true;
            }
        }

        return false;
    }
}

ToolInteractionItem::ToolInteractionItem(const Item &base) : Item(base) {}

bool ToolInteractionItem::matches(const std::string &identifier) {
    const ItemData *data = ItemDataTable::find(identifier);
    if (data == nullptr)
        return false;

    return data->mToolType == ToolType::Hoe || data->mToolType == ToolType::Axe
           || data->mToolType == ToolType::Shovel;
}

bool ToolInteractionItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                       const Vector3i &blockPosition, int32_t face,
                                       const Vector3f &clickPosition) const {
    (void) item;
    (void) face;
    (void) clickPosition;

    Level &level = owner.getLevelFor(player);
    const BlockState clicked = level.getBlockState(blockPosition.x, blockPosition.y, blockPosition.z);

    Transformation transformation;
    if (!transformationFor(getToolType(), clicked.mName, transformation))
        return false;

    if (transformation.mNeedsAirAbove
        && level.getBlockState(blockPosition.x, blockPosition.y + 1, blockPosition.z).mName != "minecraft:air")
        return false;

    const BlockState result = CopperSystem::replaceWithPair(owner, level, blockPosition, clicked,
                                                            transformation.mIdentifier);

    const Vector3f center((float) blockPosition.x + 0.5f, (float) blockPosition.y + 0.5f,
                          (float) blockPosition.z + 0.5f);

    if (transformation.mEffect == Effect::Sound) {
        owner.playLevelSound(level, LevelSoundEvent::ITEM_USE_ON, center, ":",
                             BlockStateHasher::hash(result.mName, result.mStates));
    } else {
        LevelEventPacket event;
        event.mEventId = transformation.mEffect == Effect::WaxOff ? CopperSystem::WAX_OFF_EVENT
                                                                  : CopperSystem::SCRAPE_EVENT;
        event.mPosition = center;
        event.mData = 0;
        BlockActionHandler::broadcastToViewers(owner, level, center, event);
    }

    if (transformation.mDrop != nullptr)
        owner.spawnItemActor(level, transformation.mDrop, 1, center);

    owner.damagePlayerHeldItem(player, 1);
    return true;
}
