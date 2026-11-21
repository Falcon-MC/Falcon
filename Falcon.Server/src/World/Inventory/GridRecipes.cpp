#include "World/Inventory/GridRecipes.h"

#include "World/Item/BannerPatterns.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Item/EmptyMapItem.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/Item.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/MapItem.h"
#include "World/Item/VanillaItems.h"

#include <algorithm>
#include <map>

namespace {
    const char *const GUNPOWDER = "minecraft:gunpowder";
    const char *const PAPER = "minecraft:paper";
    const char *const FIREWORK_STAR = "minecraft:firework_star";
    const char *const FIREWORK_ROCKET = "minecraft:firework_rocket";
    const char *const GLOWSTONE_DUST = "minecraft:glowstone_dust";
    const char *const DIAMOND = "minecraft:diamond";
    const char *const FIRE_CHARGE = "minecraft:fire_charge";
    const char *const GOLD_NUGGET = "minecraft:gold_nugget";
    const char *const FEATHER = "minecraft:feather";
    const char *const WRITTEN_BOOK = "minecraft:written_book";
    const char *const WRITABLE_BOOK = "minecraft:writable_book";
    const char *const COMPASS = "minecraft:compass";
    const char *const DECORATED_POT = "minecraft:decorated_pot";
    const char *const BRICK = "minecraft:brick";
    const char *const SHERDS_TAG = "minecraft:decorated_pot_sherds";
    const char *const HEADS[] = {"minecraft:skull",       "minecraft:skeleton_skull", "minecraft:wither_skeleton_skull",
                                 "minecraft:zombie_head", "minecraft:player_head",    "minecraft:creeper_head",
                                 "minecraft:dragon_head", "minecraft:piglin_head"};

    const char *const TAG_FIREWORKS = "Fireworks";
    const char *const TAG_EXPLOSIONS = "Explosions";
    const char *const TAG_FLIGHT = "Flight";
    const char *const TAG_FIREWORKS_ITEM = "FireworksItem";
    const char *const TAG_TYPE = "FireworkType";
    const char *const TAG_COLOR = "FireworkColor";
    const char *const TAG_FADE = "FireworkFade";
    const char *const TAG_FLICKER = "FireworkFlicker";
    const char *const TAG_TRAIL = "FireworkTrail";
    const char *const TAG_CUSTOM_COLOR = "customColor";
    const char *const TAG_GENERATION = "generation";
    const char *const TAG_SHERDS = "sherds";

    // the game stops at three gunpowder in a rocket, and copies only an original or its copy
    const int32_t MAX_GUNPOWDER = 3;
    const int32_t MAX_COPIED_GENERATION = 1;

    // the dye colours, from black, as the game blends them
    const uint32_t DYE_RGB[16] = {0x1d1d21, 0xb02e26, 0x5e7c16, 0x835432, 0x3c44aa, 0x8932b8, 0x169c9c, 0x9d9d97,
                                  0x474f52, 0xf38baa, 0x80c71f, 0xfed83d, 0x3ab3da, 0xc74ebd, 0xf9801d, 0xf0f0f0};

    std::map<GridRecipes::Kind, int32_t> &networkIds() {
        static std::map<GridRecipes::Kind, int32_t> ids;
        return ids;
    }

    const std::string &idOf(const ItemStack &item) {
        static const std::string none;
        return item.isAir() || item.mDefinition == nullptr ? none : item.mDefinition->getIdentifier();
    }

    bool isHead(const std::string &identifier) {
        for (const char *head: HEADS) {
            if (identifier == head)
                return true;
        }
        return false;
    }

    ItemStack single(const ItemStack &item, int32_t count) {
        ItemStack copy = item;
        copy.mCount = count;
        copy.mUsingNetId = false;
        copy.mNetId = 0;
        return copy;
    }

    // one of every filled slot
    std::vector<int32_t> oneOfEach(const std::vector<ItemStack> &grid) {
        std::vector<int32_t> required(grid.size(), 0);
        for (size_t slot = 0; slot < grid.size(); ++slot)
            required[slot] = grid[slot].isAir() ? 0 : 1;
        return required;
    }

    int32_t generationOf(const ItemStack &book) {
        return book.mTag.isCompound() ? book.mTag.getInt(TAG_GENERATION, 0) : 0;
    }

    const std::vector<int8_t> &bytesOf(const Tag &compound, const char *key) {
        static const std::vector<int8_t> none;
        const Tag *value = compound.isCompound() ? compound.get(key) : nullptr;
        return value != nullptr && value->getType() == Tag::Type::ByteArray ? value->asByteArray() : none;
    }

    ItemStack star(const GridRecipes::Make &make, int8_t type, const std::vector<int8_t> &colors,
                   const std::vector<int8_t> &fades, bool trail, bool flicker) {
        ItemStack result = make(FIREWORK_STAR, 1);
        if (result.isAir())
            return result;
        Tag explosion = Tag::ofCompound();
        explosion.putBool(TAG_TRAIL, trail);
        explosion.putBool(TAG_FLICKER, flicker);
        explosion.putByte(TAG_TYPE, type);
        explosion.put(TAG_COLOR, Tag::ofByteArray(colors));
        explosion.put(TAG_FADE, Tag::ofByteArray(fades));
        result.mTag = Tag::ofCompound();
        result.mTag.put(TAG_FIREWORKS_ITEM, std::move(explosion));
        result.mTag.putInt(TAG_CUSTOM_COLOR, GridRecipes::starColor(colors));
        return result;
    }
}

const std::vector<std::pair<GridRecipes::Kind, const char *>> &GridRecipes::uuids() {
    static const std::vector<std::pair<Kind, const char *>> list = {
            {Kind::Firework, "00000000-0000-0000-0000-000000000002"},
            {Kind::BookCloning, "d1ca6b84-338e-4f2f-9c6b-76cc8b4bd98d"},
            {Kind::MapCloning, "85939755-ba10-4d9d-a4cc-efb7a8e943c4"},
            {Kind::MapExtending, "d392b075-4ba1-40ae-8789-af868d56f6ce"},
            {Kind::MapUpgrading, "aecd2294-4b94-434b-8667-4499bb2c9327"},
            {Kind::Repair, "00000000-0000-0000-0000-000000000001"},
            {Kind::DecoratedPot, "685a742a-c42e-4a4e-88ea-5eb83fc98e5b"},
    };
    return list;
}

void GridRecipes::setNetworkId(Kind kind, int32_t networkId) {
    networkIds()[kind] = networkId;
}

GridRecipes::Kind GridRecipes::kindOf(int32_t networkId) {
    if (networkId <= 0)
        return Kind::None;
    for (const auto &entry: networkIds()) {
        if (entry.second == networkId)
            return entry.first;
    }
    return Kind::None;
}

bool GridRecipes::craft(Kind kind, const std::vector<ItemStack> &grid, const Make &make,
                        const CartographyRecipes::Maps *maps, Craft &craft) {
    switch (kind) {
        case Kind::Firework:
            return firework(grid, make, craft);
        case Kind::BookCloning:
            return bookCloning(grid, craft);
        case Kind::MapCloning:
            return mapCloning(grid, craft);
        case Kind::MapExtending:
            return maps != nullptr && mapExtending(grid, *maps, craft);
        case Kind::MapUpgrading:
            return mapUpgrading(grid, craft);
        case Kind::Repair:
            return repair(grid, make, craft);
        case Kind::DecoratedPot:
            return decoratedPot(grid, make, craft);
        case Kind::None:
            break;
    }
    return false;
}

int32_t GridRecipes::starColor(const std::vector<int8_t> &colors) {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    int32_t counted = 0;
    for (const int8_t color: colors) {
        const uint8_t index = (uint8_t) color;
        if (index > 15)
            continue;
        const uint32_t rgb = DYE_RGB[index];
        red += (float) ((rgb >> 16) & 0xFF) / 255.0f;
        green += (float) ((rgb >> 8) & 0xFF) / 255.0f;
        blue += (float) (rgb & 0xFF) / 255.0f;
        ++counted;
    }
    if (counted > 0) {
        const float share = 1.0f / (float) counted;
        red *= share;
        green *= share;
        blue *= share;
    }
    const auto channel = [](float value) { return (uint32_t) (int32_t) (std::clamp(value, 0.0f, 1.0f) * 255.0f); };
    return (int32_t) (0xFF000000u | (channel(red) << 16) | (channel(green) << 8) | channel(blue));
}

bool GridRecipes::firework(const std::vector<ItemStack> &grid, const Make &make, Craft &craft) {
    int32_t gunpowder = 0;
    int32_t paper = 0;
    int32_t modifiers = 0;
    int32_t shapes = 0;
    int8_t shape = SHAPE_SMALL;
    bool trail = false;
    bool flicker = false;
    std::vector<int8_t> dyes;
    std::vector<const ItemStack *> stars;
    for (const ItemStack &item: grid) {
        if (item.isAir())
            continue;
        const std::string &identifier = idOf(item);
        const uint8_t dye = BannerPatterns::dyeColorOf(item);
        if (identifier == GUNPOWDER) {
            ++gunpowder;
        } else if (identifier == FIREWORK_STAR) {
            stars.push_back(&item);
        } else if (dye != 0xFF) {
            dyes.push_back((int8_t) dye);
        } else if (identifier == PAPER) {
            ++paper;
        } else if (identifier == GLOWSTONE_DUST) {
            ++modifiers;
            flicker = true;
        } else if (identifier == DIAMOND) {
            ++modifiers;
            trail = true;
        } else if (identifier == FIRE_CHARGE || identifier == GOLD_NUGGET || identifier == FEATHER
                   || isHead(identifier)) {
            ++shapes;
            shape = identifier == FIRE_CHARGE   ? SHAPE_LARGE
                    : identifier == GOLD_NUGGET ? SHAPE_STAR
                    : identifier == FEATHER     ? SHAPE_BURST
                                                : SHAPE_CREEPER;
        } else {
            return false;
        }
    }
    if (gunpowder > MAX_GUNPOWDER || paper >= 2 || shapes >= 2)
        return false;

    const int32_t extras = shapes + (int32_t) dyes.size() + modifiers;
    if (gunpowder > 0 && paper == 1 && extras == 0) {
        ItemStack rocket = make(FIREWORK_ROCKET, ROCKETS);
        if (rocket.isAir())
            return false;
        std::vector<Tag> explosions;
        for (const ItemStack *each: stars) {
            const Tag *explosion = each->mTag.isCompound() ? each->mTag.get(TAG_FIREWORKS_ITEM) : nullptr;
            if (explosion != nullptr && explosion->isCompound())
                explosions.push_back(*explosion);
        }
        Tag fireworks = Tag::ofCompound();
        fireworks.put(TAG_EXPLOSIONS, Tag::ofList(Tag::Type::Compound, std::move(explosions)));
        fireworks.putByte(TAG_FLIGHT, (int8_t) gunpowder);
        rocket.mTag = Tag::ofCompound();
        rocket.mTag.put(TAG_FIREWORKS, std::move(fireworks));
        craft.mResult = std::move(rocket);
    } else if (gunpowder == 1 && paper == 0 && stars.empty() && !dyes.empty()) {
        craft.mResult = star(make, shape, dyes, {}, trail, flicker);
    } else if (gunpowder == 0 && paper == 0 && stars.size() == 1 && !dyes.empty() && extras == (int32_t) dyes.size()) {
        // the dyes fade the star; the rest of it stays as it was
        const Tag *explosion = stars.front()->mTag.isCompound() ? stars.front()->mTag.get(TAG_FIREWORKS_ITEM) : nullptr;
        if (explosion == nullptr || !explosion->isCompound())
            return false;
        craft.mResult = star(make, explosion->getByte(TAG_TYPE, 0), bytesOf(*explosion, TAG_COLOR), dyes,
                             explosion->getByte(TAG_TRAIL, 0) != 0, explosion->getByte(TAG_FLICKER, 0) != 0);
    } else {
        return false;
    }
    if (craft.mResult.isAir())
        return false;
    craft.mRequiredBySlot = oneOfEach(grid);
    return true;
}

bool GridRecipes::bookCloning(const std::vector<ItemStack> &grid, Craft &craft) {
    int32_t original = -1;
    int32_t blanks = 0;
    for (size_t slot = 0; slot < grid.size(); ++slot) {
        const ItemStack &item = grid[slot];
        if (item.isAir())
            continue;
        const std::string &identifier = idOf(item);
        if (identifier == WRITTEN_BOOK) {
            if (original >= 0 || generationOf(item) > MAX_COPIED_GENERATION)
                return false;
            original = (int32_t) slot;
        } else if (identifier == WRITABLE_BOOK) {
            ++blanks;
        } else {
            return false;
        }
    }
    if (original < 0 || blanks == 0)
        return false;

    const ItemStack &book = grid[(size_t) original];
    ItemStack copies = single(book, blanks);
    if (!copies.mTag.isCompound())
        copies.mTag = Tag::ofCompound();
    copies.mTag.putInt(TAG_GENERATION, generationOf(book) + 1);
    craft.mResult = std::move(copies);
    craft.mRequiredBySlot = oneOfEach(grid);
    craft.mRequiredBySlot[(size_t) original] = 0;
    return true;
}

bool GridRecipes::mapCloning(const std::vector<ItemStack> &grid, Craft &craft) {
    int32_t original = -1;
    int32_t blanks = 0;
    for (size_t slot = 0; slot < grid.size(); ++slot) {
        const ItemStack &item = grid[slot];
        if (item.isAir())
            continue;
        const std::string &identifier = idOf(item);
        if (identifier == MapItem::IDENTIFIER) {
            if (original >= 0)
                return false;
            original = (int32_t) slot;
        } else if (identifier == EmptyMapItem::IDENTIFIER) {
            ++blanks;
        } else {
            return false;
        }
    }
    if (original < 0 || blanks == 0)
        return false;

    craft.mResult = single(grid[(size_t) original], blanks + 1);
    craft.mRequiredBySlot = oneOfEach(grid);
    return true;
}

bool GridRecipes::mapExtending(const std::vector<ItemStack> &grid, const CartographyRecipes::Maps &maps, Craft &craft) {
    const size_t middle = 4;
    if (grid.size() < 9 || idOf(grid[middle]) != MapItem::IDENTIFIER)
        return false;
    for (size_t slot = 0; slot < grid.size(); ++slot) {
        if (slot != middle && idOf(grid[slot]) != PAPER)
            return false;
    }

    ItemStack result;
    if (!CartographyRecipes::extendMap(grid[middle], maps, result))
        return false;
    craft.mResult = std::move(result);
    craft.mRequiredBySlot = oneOfEach(grid);
    return true;
}

bool GridRecipes::mapUpgrading(const std::vector<ItemStack> &grid, Craft &craft) {
    int32_t map = -1;
    int32_t compasses = 0;
    for (size_t slot = 0; slot < grid.size(); ++slot) {
        const ItemStack &item = grid[slot];
        if (item.isAir())
            continue;
        const std::string &identifier = idOf(item);
        if (identifier == MapItem::IDENTIFIER || identifier == EmptyMapItem::IDENTIFIER) {
            if (map >= 0)
                return false;
            map = (int32_t) slot;
        } else if (identifier == COMPASS) {
            ++compasses;
        } else {
            return false;
        }
    }
    if (map < 0 || compasses != 1)
        return false;

    ItemStack result;
    if (!CartographyRecipes::upgradeMap(grid[(size_t) map], result))
        return false;
    craft.mResult = single(result, 1);
    craft.mRequiredBySlot = oneOfEach(grid);
    return true;
}

bool GridRecipes::repair(const std::vector<ItemStack> &grid, const Make &make, Craft &craft) {
    std::vector<size_t> slots;
    for (size_t slot = 0; slot < grid.size(); ++slot) {
        if (!grid[slot].isAir())
            slots.push_back(slot);
    }
    if (slots.size() != 2)
        return false;
    const ItemStack &first = grid[slots[0]];
    const ItemStack &second = grid[slots[1]];
    const std::string &identifier = idOf(first);
    if (identifier != idOf(second) || first.mCount != 1 || second.mCount != 1)
        return false;
    const Item *item = VanillaItems::fromIdentifier(identifier);
    const int32_t durability = item == nullptr ? 0 : item->getMaxDurability();
    if (durability <= 0)
        return false;

    ItemStack result = make(identifier, 1);
    if (result.isAir())
        return false;
    result.mDamage = std::max(0, first.mDamage + second.mDamage - durability - durability / 20);

    // only the curses come along, each once
    std::vector<EnchantmentInstance> curses;
    for (const ItemStack *each: {&first, &second}) {
        for (const EnchantmentInstance &enchantment: ItemEnchantments::read(*each)) {
            if (enchantment.mId != EnchantmentIds::BINDING && enchantment.mId != EnchantmentIds::VANISHING)
                continue;
            const bool kept = std::any_of(curses.begin(), curses.end(), [&](const EnchantmentInstance &curse) {
                return curse.mId == enchantment.mId;
            });
            if (!kept)
                curses.push_back(enchantment);
        }
    }
    if (!curses.empty())
        ItemEnchantments::write(result, curses);

    craft.mResult = std::move(result);
    craft.mRequiredBySlot = oneOfEach(grid);
    return true;
}

bool GridRecipes::decoratedPot(const std::vector<ItemStack> &grid, const Make &make, Craft &craft) {
    // the back, the left, the right and the front of the pot
    const size_t faces[] = {1, 3, 5, 7};
    const size_t empty[] = {0, 2, 4, 6, 8};
    if (grid.size() < 9)
        return false;
    for (const size_t slot: empty) {
        if (!grid[slot].isAir())
            return false;
    }

    std::vector<Tag> sherds;
    bool plain = true;
    for (const size_t slot: faces) {
        const std::string &identifier = idOf(grid[slot]);
        const std::vector<std::string> &tags = CraftingRecipeTable::getItemTags(identifier);
        if (identifier.empty() || std::find(tags.begin(), tags.end(), SHERDS_TAG) == tags.end())
            return false;
        plain = plain && identifier == BRICK;
        sherds.push_back(Tag::ofString(identifier));
    }

    ItemStack pot = make(DECORATED_POT, 1);
    if (pot.isAir())
        return false;
    // four bricks are the plain pot, with nothing saved
    if (!plain) {
        pot.mTag = Tag::ofCompound();
        pot.mTag.put(TAG_SHERDS, Tag::ofList(Tag::Type::String, std::move(sherds)));
    }
    craft.mResult = std::move(pot);
    craft.mRequiredBySlot = oneOfEach(grid);
    return true;
}
