#include "Server/Commands/CommandItems.h"

#include "Network/Handler/MapHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Actor.h"
#include "World/Item/DyeableItems.h"
#include "World/Item/ExplorationMaps.h"
#include "World/Item/MapItem.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>

namespace {
    const char *const FIREWORK_STAR = "minecraft:firework_star";
    const char *const FIREWORK_ROCKET = "minecraft:firework_rocket";
    const int MIN_FLIGHT = 1;
    const int MAX_FLIGHT = 3;
    // the last data value of a plain map: 2 shows the players
    const int LAST_PLAIN_MAP = 2;
    const uint32_t OPAQUE = 0xFF000000u;

    // by firework color, black first
    const uint32_t FIREWORK_COLORS[] = {0x1D1D21, 0xB02E26, 0x5E7C16, 0x835432, 0x3C44AA, 0x8932B8,
                                        0x169C9C, 0x9D9D97, 0x474F52, 0xF38BAA, 0x80C71F, 0xFED83D,
                                        0x3AB3DA, 0xC74EBD, 0xF9801D, 0xF0F0F0};
    const int FIREWORK_COLOR_COUNT = (int) (sizeof(FIREWORK_COLORS) / sizeof(FIREWORK_COLORS[0]));

    bool is(const ItemStack &item, const char *identifier) {
        return item.mDefinition != nullptr && item.mDefinition->getIdentifier() == identifier;
    }

    Tag &tagOf(ItemStack &item) {
        if (!item.mTag.isCompound())
            item.mTag = Tag::ofCompound();
        return item.mTag;
    }

    void colorStar(ItemStack &item) {
        const int8_t color = (int8_t) item.mDamage;

        Tag star = Tag::ofCompound();
        star.putByte("FireworkTrail", 0);
        star.putByte("FireworkFlicker", 0);
        star.putByte("FireworkType", 0);
        star.put("FireworkColor", Tag::ofByteArray({color}));
        star.put("FireworkFade", Tag::ofByteArray({}));

        Tag &tag = tagOf(item);
        tag.put("FireworksItem", star);
        // a color past the sixteen leaves the star black
        const uint32_t rgb = color >= 0 && color < FIREWORK_COLOR_COUNT ? FIREWORK_COLORS[color] : 0;
        tag.putInt(DyeableItems::TAG_CUSTOM_COLOR, (int32_t) (OPAQUE | rgb));
        item.mDamage = (uint8_t) color;
    }

    void fuelRocket(ItemStack &item, int data) {
        Tag fireworks = Tag::ofCompound();
        fireworks.put("Explosions", Tag::ofList(Tag::Type::Compound));
        fireworks.putByte("Flight", (int8_t) std::min(std::max(data, MIN_FLIGHT), MAX_FLIGHT));
        tagOf(item).put("Fireworks", fireworks);
    }
}

void CommandItems::applyData(ItemStack &item, int data) {
    if (is(item, FIREWORK_STAR))
        colorStar(item);
    else if (is(item, FIREWORK_ROCKET))
        fuelRocket(item, data);
}

bool CommandItems::prepareFor(ServerNetworkHandler &owner, Actor &actor, ItemStack &item, std::string &error) {
    if (!is(item, MapItem::IDENTIFIER))
        return true;

    const int32_t data = item.mDamage;
    if (const ExplorationMaps::Entry *entry = ExplorationMaps::findByAux(data)) {
        ExplorationMaps::create(owner, item, &actor, *entry);
        return true;
    }
    if (data > LAST_PLAIN_MAP) {
        error = "commands.give.map.invalidData";
        return false;
    }

    Tag &tag = tagOf(item);
    if (data == LAST_PLAIN_MAP)
        tag.putByte(MapItem::TAG_DISPLAY_PLAYERS, 1);

    const Vector3f &position = actor.getPosition();
    MapItemSavedData &map = MapItem::getMapLevel(owner, actor).getMaps().createMapSavedData(
            [&owner]() { return owner.allocateActorUniqueId(); },
            Vector3f(std::floor(position.x), std::floor(position.y), std::floor(position.z)),
            Dimension::toId(actor.getDimension()), 0);
    MapItem::setMapId(item, map);
    MapHandler::broadcastCreation(owner, owner.getWorldFor(actor), map);
    return true;
}
