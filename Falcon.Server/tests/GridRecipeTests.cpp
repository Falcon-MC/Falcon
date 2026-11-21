#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CraftingDataPacket.h"
#include "World/Inventory/GridRecipes.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/Item.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/VanillaItems.h"
#include "World/Item/MapItem.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/CrafterBlock.h"
#include "World/Level/Map/MapItemSavedData.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    using Kind = GridRecipes::Kind;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Bench {
        ServerNetworkHandler &mOwner;
        GridRecipes::Make mMake;

        explicit Bench(ServerNetworkHandler &owner)
                : mOwner(owner), mMake([&owner](const std::string &identifier, int32_t count) {
                      return owner.createItemStack(identifier, count);
                  }) {}

        ItemStack item(const std::string &identifier, int32_t count = 1) {
            ItemStack stack = mOwner.createItemStack(identifier, count);
            require(!stack.isAir(), "no item " + identifier);
            return stack;
        }

        // a 3x3 grid, row by row; an empty name leaves the slot empty
        std::vector<ItemStack> grid(const std::vector<std::string> &names) {
            std::vector<ItemStack> slots(9, ItemStack::air());
            for (size_t slot = 0; slot < names.size() && slot < slots.size(); ++slot) {
                if (!names[slot].empty())
                    slots[slot] = item(names[slot]);
            }
            return slots;
        }

        bool craft(Kind kind, const std::vector<ItemStack> &slots, GridRecipes::Craft &out,
                   const CartographyRecipes::Maps *maps = nullptr) {
            out = GridRecipes::Craft();
            return GridRecipes::craft(kind, slots, mMake, maps, out);
        }
    };

    ItemStack starOf(Bench &bench, int8_t type, const std::vector<int8_t> &colors) {
        ItemStack star = bench.item("minecraft:firework_star");
        Tag explosion = Tag::ofCompound();
        explosion.putByte("FireworkType", type);
        explosion.put("FireworkColor", Tag::ofByteArray(colors));
        explosion.put("FireworkFade", Tag::ofByteArray({}));
        explosion.putBool("FireworkTrail", true);
        explosion.putBool("FireworkFlicker", false);
        star.mTag = Tag::ofCompound();
        star.mTag.put("FireworksItem", explosion);
        return star;
    }

    // every recipe reaches the client under the id the server checks
    void declared(Bench &bench) {
        bench.mOwner.getRecipeOutputs();
        for (const auto &recipe: GridRecipes::uuids()) {
            bool sent = false;
            for (const MultiRecipeEntry &multi: bench.mOwner.getCachedCraftingData().mMultiRecipes) {
                if (multi.mUuid == Uuid::fromString(recipe.second)) {
                    sent = GridRecipes::kindOf((int32_t) multi.mRecipeNetId) == recipe.first;
                    break;
                }
            }
            require(sent, std::string("the recipe ") + recipe.second + " was not sent under its id");
        }
    }

    void fireworks(Bench &bench) {
        GridRecipes::Craft out;
        // gunpowder and paper: three rockets flying as far as the gunpowder, carrying the stars
        std::vector<ItemStack> rocket = bench.grid({"minecraft:gunpowder", "minecraft:gunpowder", "minecraft:paper"});
        rocket[3] = starOf(bench, GridRecipes::SHAPE_STAR, {1});
        require(bench.craft(Kind::Firework, rocket, out), "gunpowder and paper made no rocket");
        require(out.mResult.mDefinition->getIdentifier() == "minecraft:firework_rocket" && out.mResult.mCount == 3,
                "the rocket craft did not make three rockets");
        const Tag *fireworks = out.mResult.mTag.get("Fireworks");
        require(fireworks != nullptr && fireworks->getByte("Flight", 0) == 2, "the rockets do not fly as far as the gunpowder");
        require(fireworks->get("Explosions")->getList().size() == 1, "the rockets do not carry the star");
        require(out.mRequiredBySlot[0] == 1 && out.mRequiredBySlot[3] == 1 && out.mRequiredBySlot[4] == 0,
                "the rocket craft does not take one of each filled slot");

        // gunpowder and dyes: a star with the shape item, the trail and the flicker
        std::vector<ItemStack> star = bench.grid({"minecraft:gunpowder", "minecraft:red_dye", "minecraft:white_dye",
                                                  "minecraft:diamond", "minecraft:glowstone_dust", "minecraft:feather"});
        require(bench.craft(Kind::Firework, star, out), "gunpowder and dyes made no star");
        const Tag *explosion = out.mResult.mTag.get("FireworksItem");
        require(out.mResult.mDefinition->getIdentifier() == "minecraft:firework_star" && explosion != nullptr,
                "the star craft made no star");
        require(explosion->getByte("FireworkType", -1) == GridRecipes::SHAPE_BURST, "a feather did not make a burst");
        require(explosion->get("FireworkColor")->asByteArray() == std::vector<int8_t>({1, 15}),
                "the star does not carry its dyes");
        require(explosion->getByte("FireworkTrail", 0) == 1 && explosion->getByte("FireworkFlicker", 0) == 1,
                "the diamond and the glowstone did not give the trail and the flicker");
        require(out.mResult.mTag.getInt("customColor", 0) == GridRecipes::starColor({1, 15}),
                "the star does not show its dyes' colour");

        // dyes on a star: the same star, faded
        std::vector<ItemStack> fade = bench.grid({"", "minecraft:blue_dye"});
        fade[0] = starOf(bench, GridRecipes::SHAPE_LARGE, {2});
        require(bench.craft(Kind::Firework, fade, out), "dyes did not fade a star");
        explosion = out.mResult.mTag.get("FireworksItem");
        require(explosion->getByte("FireworkType", -1) == GridRecipes::SHAPE_LARGE
                        && explosion->get("FireworkColor")->asByteArray() == std::vector<int8_t>({2})
                        && explosion->get("FireworkFade")->asByteArray() == std::vector<int8_t>({4})
                        && explosion->getByte("FireworkTrail", 0) == 1,
                "the faded star lost its own look");

        const std::vector<std::vector<std::string>> refused = {
                {"minecraft:gunpowder", "minecraft:gunpowder", "minecraft:gunpowder", "minecraft:gunpowder",
                 "minecraft:paper"},
                {"minecraft:gunpowder", "minecraft:paper", "minecraft:paper"},
                {"minecraft:gunpowder", "minecraft:red_dye", "minecraft:feather", "minecraft:gold_nugget"},
                {"minecraft:gunpowder", "minecraft:paper", "minecraft:red_dye"},
                {"minecraft:gunpowder", "minecraft:paper", "minecraft:stick"},
                {"minecraft:gunpowder"},
        };
        for (const auto &names: refused)
            require(!bench.craft(Kind::Firework, bench.grid(names), out), "a firework came out of a wrong grid");
    }

    void books(Bench &bench) {
        GridRecipes::Craft out;
        std::vector<ItemStack> slots = bench.grid({"minecraft:written_book", "minecraft:writable_book", "", "minecraft:writable_book"});
        slots[0].mTag = Tag::ofCompound();
        slots[0].mTag.putString("title", "Tale");
        require(bench.craft(Kind::BookCloning, slots, out), "a book was not copied");
        require(out.mResult.mCount == 2 && out.mResult.mTag.getString("title") == "Tale"
                        && out.mResult.mTag.getInt("generation", -1) == 1,
                "the copies are not two of the book a generation on");
        require(out.mRequiredBySlot[0] == 0 && out.mRequiredBySlot[1] == 1 && out.mRequiredBySlot[3] == 1,
                "the original was used up or a book and quill was not");

        slots[0].mTag.putInt("generation", 2);
        require(!bench.craft(Kind::BookCloning, slots, out), "a copy of a copy was copied");
        require(!bench.craft(Kind::BookCloning, bench.grid({"minecraft:written_book", "minecraft:written_book",
                                                             "minecraft:writable_book"}),
                             out),
                "two written books were copied");
        require(!bench.craft(Kind::BookCloning, bench.grid({"minecraft:written_book"}), out), "a lone book was copied");
    }

    void maps(Bench &bench) {
        GridRecipes::Craft out;
        MapItemSavedData data(7);
        CartographyRecipes::Maps access;
        access.mGetMap = [&data](int64_t mapId) { return mapId == 7 ? &data : nullptr; };
        access.mExpand = [](int64_t, bool) { return (int64_t) 70; };

        const auto filled = [&bench]() {
            ItemStack map = bench.item(MapItem::IDENTIFIER);
            map.mTag = Tag::ofCompound();
            map.mTag.putLong(MapItem::TAG_MAP_ID, 7);
            return map;
        };

        std::vector<ItemStack> clone = bench.grid({"", "minecraft:empty_map", "minecraft:empty_map"});
        clone[0] = filled();
        require(bench.craft(Kind::MapCloning, clone, out) && out.mResult.mCount == 3
                        && MapItem::getMapId(out.mResult) == 7,
                "a map and two empty maps did not make three of it");
        clone[3] = bench.item("minecraft:paper");
        require(!bench.craft(Kind::MapCloning, clone, out), "paper went into a map copy");

        std::vector<ItemStack> extend = bench.grid({"minecraft:paper", "minecraft:paper", "minecraft:paper", "minecraft:paper",
                                                    "", "minecraft:paper", "minecraft:paper", "minecraft:paper",
                                                    "minecraft:paper"});
        extend[4] = filled();
        require(!bench.craft(Kind::MapExtending, extend, out), "a map grew without the maps to read");
        require(bench.craft(Kind::MapExtending, extend, out, &access) && MapItem::getMapId(out.mResult) == 70,
                "a map in eight paper did not grow");
        data.setLocked(true);
        require(!bench.craft(Kind::MapExtending, extend, out, &access), "a locked map grew");
        data.setLocked(false);
        std::swap(extend[4], extend[0]);
        require(!bench.craft(Kind::MapExtending, extend, out, &access), "a map off the middle grew");

        std::vector<ItemStack> upgrade = bench.grid({"minecraft:compass"});
        upgrade[1] = filled();
        require(bench.craft(Kind::MapUpgrading, upgrade, out)
                        && out.mResult.mTag.getByte(MapItem::TAG_DISPLAY_PLAYERS, 0) == 1,
                "a compass did not make the map show the players");
        upgrade[2] = bench.item("minecraft:compass");
        require(!bench.craft(Kind::MapUpgrading, upgrade, out), "two compasses upgraded a map");
    }

    void repairs(Bench &bench) {
        GridRecipes::Craft out;
        std::vector<ItemStack> slots = bench.grid({"minecraft:iron_pickaxe", "", "", "minecraft:iron_pickaxe"});
        const int32_t uses = VanillaItems::fromIdentifier("minecraft:iron_pickaxe")->getMaxDurability();
        // 50 and 100 uses left: 150 and a twentieth more
        slots[0].mDamage = uses - 50;
        slots[3].mDamage = uses - 100;
        ItemEnchantments::write(slots[0], {EnchantmentInstance{EnchantmentIds::EFFICIENCY, 3},
                                           EnchantmentInstance{EnchantmentIds::VANISHING, 1}});
        require(bench.craft(Kind::Repair, slots, out), "two worn pickaxes were not repaired");
        require(out.mResult.mDamage == uses - 150 - uses / 20,
                "the repair did not add both durabilities and a twentieth: " + std::to_string(out.mResult.mDamage));
        require(ItemEnchantments::getLevel(out.mResult, EnchantmentIds::VANISHING) == 1
                        && ItemEnchantments::getLevel(out.mResult, EnchantmentIds::EFFICIENCY) == 0,
                "the repair did not keep only the curse");

        slots[3] = bench.item("minecraft:stone_pickaxe");
        require(!bench.craft(Kind::Repair, slots, out), "two kinds of tools were repaired together");
        require(!bench.craft(Kind::Repair, bench.grid({"minecraft:stick", "minecraft:stick"}), out),
                "sticks were repaired");
        require(!bench.craft(Kind::Repair, bench.grid({"minecraft:iron_pickaxe"}), out), "a lone tool was repaired");
    }

    void pots(Bench &bench) {
        GridRecipes::Craft out;
        require(bench.craft(Kind::DecoratedPot,
                            bench.grid({"", "minecraft:angler_pottery_sherd", "", "minecraft:brick", "", "minecraft:brick",
                                        "", "minecraft:skull_pottery_sherd"}),
                            out),
                "sherds and bricks in a cross made no pot");
        const Tag *sherds = out.mResult.mTag.get("sherds");
        require(out.mResult.mDefinition->getIdentifier() == "minecraft:decorated_pot" && sherds != nullptr
                        && sherds->getList().size() == 4 && sherds->getList()[0].asString() == "minecraft:angler_pottery_sherd"
                        && sherds->getList()[3].asString() == "minecraft:skull_pottery_sherd",
                "the pot does not show its sherds in order");

        require(bench.craft(Kind::DecoratedPot,
                            bench.grid({"", "minecraft:brick", "", "minecraft:brick", "", "minecraft:brick", "",
                                        "minecraft:brick"}),
                            out)
                        && !out.mResult.mTag.isCompound(),
                "four bricks did not make the plain pot");
        require(!bench.craft(Kind::DecoratedPot,
                             bench.grid({"minecraft:brick", "minecraft:brick", "", "minecraft:brick", "", "minecraft:brick",
                                         "", "minecraft:brick"}),
                             out),
                "a pot came out of a full corner");
        require(!bench.craft(Kind::DecoratedPot,
                             bench.grid({"", "minecraft:stick", "", "minecraft:brick", "", "minecraft:brick", "",
                                         "minecraft:brick"}),
                             out),
                "a stick went into a pot");
    }

    // the request a player sends: three rockets out of the 2x2 grid, the ingredients used up
    void request(Bench &bench) {
        int32_t netId = 0;
        for (const auto &recipe: GridRecipes::uuids()) {
            for (const MultiRecipeEntry &multi: bench.mOwner.getCachedCraftingData().mMultiRecipes) {
                if (recipe.first == Kind::Firework && multi.mUuid == Uuid::fromString(recipe.second))
                    netId = (int32_t) multi.mRecipeNetId;
            }
        }
        PlayerInventory inventory;
        inventory.setCraftingItem(0, bench.item("minecraft:gunpowder"));
        inventory.setCraftingItem(1, bench.item("minecraft:paper"));

        const auto slotOf = [](ContainerSlotType container, int32_t slot) {
            ItemStackRequestSlotData data;
            data.mContainer = container;
            data.mContainerName.mContainer = container;
            data.mSlot = slot;
            return data;
        };
        ItemStackRequest request;
        request.mRequestId = -1;
        ItemStackRequestAction craft;
        craft.mType = ItemStackRequestActionType::CraftRecipe;
        craft.mRecipeNetworkId = netId;
        craft.mNumberOfRequestedCrafts = 1;
        request.mActions.push_back(craft);
        for (int32_t slot = 0; slot < 2; ++slot) {
            ItemStackRequestAction consume;
            consume.mType = ItemStackRequestActionType::Consume;
            consume.mCount = 1;
            consume.mSource = slotOf(ContainerSlotType::CraftingInput, PlayerInventory::CRAFTING_NETWORK_SLOT_FIRST + slot);
            request.mActions.push_back(consume);
        }
        ItemStackRequestAction take;
        take.mType = ItemStackRequestActionType::Take;
        take.mCount = 3;
        take.mSource = slotOf(ContainerSlotType::CreatedOutput, 50);
        take.mDestination = slotOf(ContainerSlotType::Inventory, 0);
        request.mActions.push_back(take);

        const std::vector<CreativeItemData> creative;
        const ItemStackResponseEntry entry = ItemStackRequestHandler::execute(
                inventory, request, creative, bench.mOwner.getRecipeOutputs(), bench.mOwner.getRecipeSourceIndices(),
                false, false, nullptr, nullptr, &bench.mOwner.getCodecContext());
        require(entry.mResult == ItemStackRequestHandler::RESULT_OK, "a rocket craft was refused");
        require(inventory.getItem(0).mCount == 3 && inventory.getItem(0).mDefinition->getIdentifier() == "minecraft:firework_rocket",
                "the player did not get three rockets");
    }

    // the crafter makes them too, and hands back the book it copied
    void crafter(Bench &bench) {
        CrafterBlockActor copier;
        ItemStack book = bench.item("minecraft:written_book");
        book.mTag = Tag::ofCompound();
        book.mTag.putString("title", "Tale");
        copier.getInventory().setContainerItem(0, book);
        copier.getInventory().setContainerItem(1, bench.item("minecraft:writable_book"));
        const std::vector<ItemStack> results = CrafterBlock::craftResults(bench.mOwner, copier);
        require(results.size() == 2 && results[0].mTag.getInt("generation", -1) == 1
                        && results[1].mDefinition->getIdentifier() == "minecraft:written_book"
                        && results[1].mTag.getString("title") == "Tale",
                "the crafter did not give the copy and the original");

        CrafterBlockActor rockets;
        rockets.getInventory().setContainerItem(4, bench.item("minecraft:gunpowder"));
        rockets.getInventory().setContainerItem(5, bench.item("minecraft:paper"));
        const std::vector<ItemStack> made = CrafterBlock::craftResults(bench.mOwner, rockets);
        require(made.size() == 1 && made[0].mCount == 3, "the crafter made no rockets");
    }
}

int main() {
    try {
        ServerNetworkHandler owner("Grid recipes", "", 1);
        Bench bench(owner);
        declared(bench);
        fireworks(bench);
        books(bench);
        maps(bench);
        repairs(bench);
        pots(bench);
        request(bench);
        crafter(bench);
    } catch (const std::exception &error) {
        std::clog << "Grid recipe regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
