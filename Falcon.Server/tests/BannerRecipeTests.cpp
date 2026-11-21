#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CraftingDataPacket.h"
#include "World/Inventory/BannerRecipes.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/CrafterBlock.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Bench {
        ServerNetworkHandler &mOwner;
        int32_t mDuplicate = 0;
        int32_t mShield = 0;

        ItemStack item(const std::string &identifier, int32_t count = 1, int32_t damage = 0) {
            ItemStack stack;
            stack.mDefinition = mOwner.getItemDefinitions().getDefinition(identifier);
            require(stack.mDefinition != nullptr, "no item " + identifier);
            stack.mCount = count;
            stack.mDamage = damage;
            return stack;
        }

        // a banner of the given colour with one layer per pattern
        ItemStack banner(int32_t color, const std::vector<std::string> &patterns, int32_t count = 1) {
            ItemStack stack = item("minecraft:banner", count, color);
            if (patterns.empty())
                return stack;
            std::vector<Tag> layers;
            for (const std::string &pattern: patterns) {
                Tag layer = Tag::ofCompound();
                layer.putString("Pattern", pattern);
                layer.putInt("Color", 0);
                layers.push_back(std::move(layer));
            }
            stack.mTag = Tag::ofCompound();
            stack.mTag.put("Patterns", Tag::ofList(Tag::Type::Compound, std::move(layers)));
            return stack;
        }

        ItemStack ominous() {
            ItemStack stack = item("minecraft:banner", 1, 15);
            stack.mTag = Tag::ofCompound();
            stack.mTag.putInt("Type", 1);
            return stack;
        }

        static ItemStackRequestSlotData slotOf(ContainerSlotType container, int32_t slot) {
            ItemStackRequestSlotData data;
            data.mContainer = container;
            data.mContainerName.mContainer = container;
            data.mSlot = slot;
            return data;
        }

        // the 2x2 grid's slots, the craft taking one of each listed slot per craft and the whole output
        bool craft(PlayerInventory &inventory, int32_t netId, const std::vector<int32_t> &consumed, int32_t taken,
                   int32_t crafts, ItemStack &crafted) {
            ItemStackRequest request;
            request.mRequestId = -1;
            ItemStackRequestAction craftAction;
            craftAction.mType = ItemStackRequestActionType::CraftRecipe;
            craftAction.mRecipeNetworkId = netId;
            craftAction.mNumberOfRequestedCrafts = crafts;
            request.mActions.push_back(craftAction);
            for (const int32_t slot: consumed) {
                ItemStackRequestAction consume;
                consume.mType = ItemStackRequestActionType::Consume;
                consume.mCount = crafts;
                consume.mSource = slotOf(ContainerSlotType::CraftingInput,
                                         PlayerInventory::CRAFTING_NETWORK_SLOT_FIRST + slot);
                request.mActions.push_back(consume);
            }
            ItemStackRequestAction take;
            take.mType = ItemStackRequestActionType::Take;
            take.mCount = taken;
            take.mSource = slotOf(ContainerSlotType::CreatedOutput, 50);
            take.mDestination = slotOf(ContainerSlotType::Inventory, 0);
            request.mActions.push_back(take);

            const std::vector<CreativeItemData> creative;
            crafted = ItemStack::air();
            const ItemStackResponseEntry entry = ItemStackRequestHandler::execute(
                    inventory, request, creative, mOwner.getRecipeOutputs(), mOwner.getRecipeSourceIndices(), false,
                    false, nullptr, nullptr, &mOwner.getCodecContext(), nullptr, 0, false, nullptr, false, nullptr,
                    nullptr, nullptr, nullptr, nullptr, &crafted);
            return entry.mResult == ItemStackRequestHandler::RESULT_OK;
        }

        bool craft(PlayerInventory &inventory, int32_t netId, const std::vector<int32_t> &consumed, int32_t taken) {
            ItemStack crafted;
            return craft(inventory, netId, consumed, taken, 1, crafted);
        }
    };

    // the client learns both recipes by their uuids, under the ids the server checks
    void declared(Bench &bench) {
        bench.mOwner.getRecipeOutputs();
        bool duplicate = false;
        bool shield = false;
        for (const MultiRecipeEntry &multi: bench.mOwner.getCachedCraftingData().mMultiRecipes) {
            if (multi.mUuid == Uuid::fromString(BannerRecipes::DUPLICATE_UUID)) {
                duplicate = true;
                bench.mDuplicate = (int32_t) multi.mRecipeNetId;
            }
            if (multi.mUuid == Uuid::fromString(BannerRecipes::SHIELD_UUID)) {
                shield = true;
                bench.mShield = (int32_t) multi.mRecipeNetId;
            }
        }
        require(duplicate && shield, "the banner recipes were not sent to the client");
        require(BannerRecipes::kindOf(bench.mDuplicate) == BannerRecipes::Kind::Duplicate
                        && BannerRecipes::kindOf(bench.mShield) == BannerRecipes::Kind::Shield,
                "the server does not know the ids it sent");
    }

    // a patterned banner and a blank one of its colour make two of the patterned one
    void copying(Bench &bench) {
        PlayerInventory inventory;
        const ItemStack patterned = bench.banner(1, {"cre", "bri"});
        inventory.setCraftingItem(0, patterned);
        inventory.setCraftingItem(3, bench.banner(1, {}));
        ItemStack crafted;
        require(bench.craft(inventory, bench.mDuplicate, {0, 3}, 2, 1, crafted), "a banner copy was refused");
        require(crafted.mCount == 2 && crafted.mDamage == 1 && crafted.mTag == patterned.mTag,
                "the copy is not two of the patterned banner");
        require(inventory.getCraftingContents()[0].isAir() && inventory.getCraftingContents()[3].isAir(),
                "the copy did not use up both banners");

        // the saved Base counts over the item's colour
        PlayerInventory based;
        ItemStack saved = bench.banner(0, {"cre"});
        saved.mTag.putInt("Base", 4);
        based.setCraftingItem(0, saved);
        based.setCraftingItem(1, bench.banner(4, {}));
        require(bench.craft(based, bench.mDuplicate, {0, 1}, 2), "a banner saved as yellow did not copy on yellow");

        // as many copies as both banners allow
        PlayerInventory stacked;
        stacked.setCraftingItem(0, bench.banner(2, {"cre"}, 3));
        stacked.setCraftingItem(1, bench.banner(2, {}, 3));
        require(bench.craft(stacked, bench.mDuplicate, {0, 1}, 6, 3, crafted) && stacked.getItem(0).mCount == 6,
                "three copies at once were not six banners");
    }

    void refusedCopies(Bench &bench) {
        const std::vector<std::pair<const char *, std::vector<ItemStack>>> grids = {
                {"two colours", {bench.banner(1, {"cre"}), bench.banner(2, {})}},
                {"two patterned banners", {bench.banner(1, {"cre"}), bench.banner(1, {"bri"})}},
                {"two blank banners", {bench.banner(1, {}), bench.banner(1, {})}},
                {"a single patterned banner", {bench.banner(1, {"cre"})}},
                {"an ominous banner", {bench.ominous(), bench.banner(15, {})}},
                {"a stick beside them", {bench.banner(1, {"cre"}), bench.banner(1, {}), bench.item("minecraft:stick")}},
                {"a blank banner first, of another colour", {bench.banner(2, {}), bench.banner(1, {"cre"})}},
                {"an ominous banner as the blank one", {bench.banner(15, {"cre"}), bench.ominous()}},
                {"three banners", {bench.banner(1, {"cre"}), bench.banner(1, {}), bench.banner(1, {})}},
        };
        for (const auto &grid: grids) {
            PlayerInventory inventory;
            std::vector<int32_t> slots;
            for (size_t slot = 0; slot < grid.second.size(); ++slot) {
                inventory.setCraftingItem((int32_t) slot, grid.second[slot]);
                slots.push_back((int32_t) slot);
            }
            BannerRecipes::Craft craft;
            require(!BannerRecipes::duplicate(grid.second, craft) && !bench.craft(inventory, bench.mDuplicate, slots, 2),
                    std::string("a banner copy was made from ") + grid.first);
        }
    }

    // the shield keeps its own data and takes the banner's layers, with the banner's colour as Base
    void decorating(Bench &bench) {
        PlayerInventory inventory;
        ItemStack shield = bench.item("minecraft:shield");
        shield.mTag = Tag::ofCompound();
        shield.mTag.putInt("Damage", 12);
        inventory.setCraftingItem(0, shield);
        const ItemStack banner = bench.banner(14, {"cre", "bri"});
        inventory.setCraftingItem(1, banner);
        // anything else in the grid stays where it is
        inventory.setCraftingItem(2, bench.item("minecraft:stick"));
        ItemStack crafted;
        require(bench.craft(inventory, bench.mShield, {0, 1}, 1, 1, crafted), "a shield decoration was refused");
        require(crafted.mDefinition->getIdentifier() == "minecraft:shield" && crafted.mCount == 1,
                "the decoration did not make one shield");
        require(crafted.mTag.getInt("Base", -1) == 14, "the shield's Base is not the banner's colour");
        require(crafted.mTag.get("Patterns") != nullptr && *crafted.mTag.get("Patterns") == *banner.mTag.get("Patterns"),
                "the shield did not take the banner's layers");
        require(crafted.mTag.getInt("Damage", 0) == 12, "the shield lost its own data");
        require(!inventory.getCraftingContents()[2].isAir(), "the stick was used up");

        // an ominous banner gives its illager layer
        PlayerInventory raid;
        raid.setCraftingItem(0, bench.item("minecraft:shield"));
        raid.setCraftingItem(1, bench.ominous());
        require(bench.craft(raid, bench.mShield, {0, 1}, 1, 1, crafted), "an ominous banner did not go on a shield");
        const Tag *layers = crafted.mTag.get("Patterns");
        require(layers != nullptr && layers->getList().size() == 1
                        && layers->getList()[0].getString("Pattern") == "ill"
                        && layers->getList()[0].getInt("Color", 0) == 15,
                "the ominous shield has no illager layer");
        require(crafted.mTag.getInt("Base", -1) == 15, "the ominous shield is not on white");
    }

    void refusedShields(Bench &bench) {
        ItemStack decorated = bench.item("minecraft:shield");
        decorated.mTag = Tag::ofCompound();
        decorated.mTag.putInt("Base", 3);
        const std::vector<std::pair<const char *, std::vector<ItemStack>>> grids = {
                {"a decorated shield", {decorated, bench.banner(1, {"cre"})}},
                {"a banner of seven layers",
                 {bench.item("minecraft:shield"), bench.banner(1, {"cre", "bri", "bo", "bs", "ts", "ls", "rs"})}},
                {"two banners", {bench.item("minecraft:shield"), bench.banner(1, {}), bench.banner(2, {})}},
                {"two shields", {bench.item("minecraft:shield"), bench.item("minecraft:shield"), bench.banner(1, {})}},
                {"no banner", {bench.item("minecraft:shield")}},
        };
        for (const auto &grid: grids) {
            PlayerInventory inventory;
            for (size_t slot = 0; slot < grid.second.size(); ++slot)
                inventory.setCraftingItem((int32_t) slot, grid.second[slot]);
            BannerRecipes::Craft craft;
            require(!BannerRecipes::decorateShield(grid.second, craft) && !bench.craft(inventory, bench.mShield, {0, 1}, 1),
                    std::string("a shield was decorated with ") + grid.first);
        }

        // six layers still fit
        PlayerInventory six;
        six.setCraftingItem(0, bench.item("minecraft:shield"));
        six.setCraftingItem(1, bench.banner(1, {"cre", "bri", "bo", "bs", "ts", "ls"}));
        require(bench.craft(six, bench.mShield, {0, 1}, 1), "a banner of six layers did not go on a shield");
    }

    // the crafter makes both too, from its own nine slots
    void crafter(Bench &bench) {
        CrafterBlockActor copier;
        const ItemStack patterned = bench.banner(3, {"cre"});
        copier.getInventory().setContainerItem(4, patterned);
        copier.getInventory().setContainerItem(8, bench.banner(3, {}));
        const std::vector<ItemStack> copies = CrafterBlock::craftResults(bench.mOwner, copier);
        require(copies.size() == 1 && copies[0].mCount == 2 && copies[0].mTag == patterned.mTag,
                "the crafter did not copy the banner");

        CrafterBlockActor decorator;
        decorator.getInventory().setContainerItem(0, bench.item("minecraft:shield"));
        decorator.getInventory().setContainerItem(1, bench.banner(6, {"cre"}));
        const std::vector<ItemStack> shields = CrafterBlock::craftResults(bench.mOwner, decorator);
        require(shields.size() == 1 && shields[0].mDefinition->getIdentifier() == "minecraft:shield"
                        && shields[0].mTag.getInt("Base", -1) == 6,
                "the crafter did not decorate the shield");

        CrafterBlockActor mismatched;
        mismatched.getInventory().setContainerItem(0, bench.banner(3, {"cre"}));
        mismatched.getInventory().setContainerItem(1, bench.banner(4, {}));
        require(CrafterBlock::craftResults(bench.mOwner, mismatched).empty(),
                "the crafter copied a banner onto another colour");
    }
}

int main() {
    try {
        ServerNetworkHandler owner("Banner recipes", "", 1);
        Bench bench{owner};
        declared(bench);
        copying(bench);
        refusedCopies(bench);
        decorating(bench);
        refusedShields(bench);
        crafter(bench);
    } catch (const std::exception &error) {
        std::clog << "Banner recipe regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
