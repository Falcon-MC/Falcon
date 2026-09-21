#include "Item/Items/ToolInteractionItem.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Item/ItemClassRegistry.h"
#include "Item/ItemData.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <string>
#include <vector>

FALCON_REGISTER_ITEM(ToolInteractionItem, 120);

namespace {
    constexpr int32_t COPPER_WAX_OFF = 2031;
    constexpr int32_t COPPER_SCRAPE = 2032;

    const std::string PREFIX = "minecraft:";
    const std::string STRIPPED = "minecraft:stripped_";
    const std::string WAXED = "minecraft:waxed_";
    const char *const OXIDATION_STAGES[] = {"exposed_", "weathered_", "oxidized_"};
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

    std::string withoutWaxOf(const std::string &identifier) {
        if (!startsWith(identifier, WAXED))
            return std::string();

        const std::string candidate = PREFIX + identifier.substr(WAXED.size());
        return exists(candidate) ? candidate : std::string();
    }

    std::string scrapedOf(const std::string &identifier) {
        for (size_t stage = 0; stage < 3; stage++) {
            const std::string prefix = PREFIX + OXIDATION_STAGES[stage];
            if (!startsWith(identifier, prefix))
                continue;

            const std::string rest = identifier.substr(prefix.size());
            std::string candidate = stage == 0 ? PREFIX + rest : PREFIX + OXIDATION_STAGES[stage - 1] + rest;
            if (stage == 0 && !exists(candidate))
                candidate += "_block";

            return exists(candidate) ? candidate : std::string();
        }

        return std::string();
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

            out.mIdentifier = withoutWaxOf(identifier);
            if (!out.mIdentifier.empty()) {
                out.mEffect = Effect::WaxOff;
                return true;
            }

            out.mIdentifier = scrapedOf(identifier);
            if (!out.mIdentifier.empty()) {
                out.mEffect = Effect::Scrape;
                return true;
            }
        }

        return false;
    }

    BlockState transform(const BlockState &source, const std::string &identifier) {
        const Block *block = VanillaBlocks::fromIdentifier(identifier);
        BlockState result = block == nullptr ? BlockState(identifier) : block->toBlockState();

        const std::vector<std::string> keys = result.mStates.getKeys();
        for (const std::string &key: keys) {
            const Tag *value = source.mStates.get(key);
            if (value != nullptr)
                result.mStates.put(key, *value);
        }

        return result;
    }

    void replace(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state) {
        level.setBlockState(position.x, position.y, position.z, state);
        BlockActionHandler::broadcastBlockUpdate(owner, level, position, state);
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

    const BlockState result = transform(clicked, transformation.mIdentifier);
    replace(owner, level, blockPosition, result);

    if (clicked.mStates.contains("upper_block_bit")) {
        const int32_t offset = clicked.mStates.getByte("upper_block_bit", 0) != 0 ? -1 : 1;
        const Vector3i other(blockPosition.x, blockPosition.y + offset, blockPosition.z);
        const BlockState otherState = level.getBlockState(other.x, other.y, other.z);
        if (otherState.mName == clicked.mName)
            replace(owner, level, other, transform(otherState, transformation.mIdentifier));
    }

    const Vector3f center((float) blockPosition.x + 0.5f, (float) blockPosition.y + 0.5f,
                          (float) blockPosition.z + 0.5f);

    if (transformation.mEffect == Effect::Sound) {
        owner.playLevelSound(level, LevelSoundEvent::ITEM_USE_ON, center, ":",
                             BlockStateHasher::hash(result.mName, result.mStates));
    } else {
        LevelEventPacket event;
        event.mEventId = transformation.mEffect == Effect::WaxOff ? COPPER_WAX_OFF : COPPER_SCRAPE;
        event.mPosition = center;
        event.mData = 0;
        BlockActionHandler::broadcastToViewers(owner, level, center, event);
    }

    if (transformation.mDrop != nullptr)
        owner.spawnItemActor(level, transformation.mDrop, 1, center);

    owner.damagePlayerHeldItem(player, 1);
    return true;
}
