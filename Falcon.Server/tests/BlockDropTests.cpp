#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/VanillaItems.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/BannerBlockActor.h"
#include "World/Level/Block/Actor/BedBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Inventory/FurnaceInventory.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const Vector3i AT(8, 64, 8);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Fixture {
        ServerNetworkHandler mOwner{"Block drops", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Fixture() {
            mLevel.getChunk(0, 0);
        }

        std::vector<ItemStack> drops(const BlockState &state) {
            mLevel.setBlockState(AT.x, AT.y, AT.z, state);
            return BlockActionHandler::blockDrops(mOwner, &mLevel, AT, state, ItemStack::air());
        }

        static std::string name(const ItemStack &item) {
            return item.mDefinition == nullptr ? std::string() : item.mDefinition->getIdentifier();
        }
    };

    struct Harvest {
        const char *block;
        const char *state;
        int age;
        const char *primary;
        int minimum;
        int maximum;
        const char *extra = nullptr;
        int extraMaximum = 0;
    };

    void cropDrops() {
        Fixture fixture;
        for (const Harvest &crop:
             std::initializer_list<Harvest>{{"potatoes", "growth", 0, "potato", 1, 1},
                                            {"potatoes", "growth", 7, "potato", 1, 4, "poisonous_potato", 1},
                                            {"carrots", "growth", 6, "carrot", 1, 1},
                                            {"carrots", "growth", 7, "carrot", 1, 4},
                                            {"wheat", "growth", 6, "wheat_seeds", 1, 1},
                                            {"wheat", "growth", 7, "wheat", 1, 1, "wheat_seeds", 3},
                                            {"beetroot", "growth", 6, "beetroot_seeds", 1, 1},
                                            {"beetroot", "growth", 7, "beetroot", 1, 2, "beetroot_seeds", 3},
                                            {"melon_stem", "growth", 7, "melon_seeds", 0, 3},
                                            {"pumpkin_stem", "growth", 7, "pumpkin_seeds", 0, 3},
                                            {"nether_wart", "age", 0, "nether_wart", 1, 1},
                                            {"nether_wart", "age", 3, "nether_wart", 2, 4},
                                            {"torchflower_crop", "growth", 1, "torchflower_seeds", 1, 1},
                                            {"pitcher_crop", "growth", 2, "pitcher_pod", 1, 1},
                                            {"pitcher_crop", "growth", 4, "pitcher_plant", 1, 1},
                                            {"sweet_berry_bush", "growth", 0, "sweet_berries", 0, 0},
                                            {"sweet_berry_bush", "growth", 2, "sweet_berries", 1, 2},
                                            {"sweet_berry_bush", "growth", 3, "sweet_berries", 2, 3},
                                            {"cocoa", "age", 0, "cocoa_beans", 1, 1},
                                            {"cocoa", "age", 2, "cocoa_beans", 3, 3},
                                            {"melon_block", nullptr, 0, "melon_slice", 3, 7}}) {
            Tag states = Tag::ofCompound();
            if (crop.state)
                states.putInt(crop.state, crop.age);
            const BlockState state(std::string("minecraft:") + crop.block, states);
            for (int trial = 0; trial < 32; ++trial) {
                int primary = 0;
                for (const ItemStack &drop: fixture.drops(state)) {
                    if (Fixture::name(drop) == state.mName)
                        require(drop.mBlockDefinition == nullptr,
                                state.mName + " attached the crop runtime ID to its harvest");

                    if (Fixture::name(drop) == std::string("minecraft:") + crop.primary)
                        primary += drop.mCount;
                    else
                        require(crop.extra && Fixture::name(drop) == std::string("minecraft:") + crop.extra
                                        && drop.mCount > 0 && drop.mCount <= crop.extraMaximum,
                                state.mName + " dropped " + Fixture::name(drop));
                }
                require(primary >= crop.minimum && primary <= crop.maximum, state.mName + " harvest count");
            }
        }
    }

    void cropToolsAndFlowers() {
        Fixture fixture;
        const auto melon = BlockState("minecraft:melon_block");
        ItemStack tool = fixture.mOwner.createItemStack("minecraft:diamond_axe", 1);
        ItemEnchantments::write(tool, {{EnchantmentIds::SILK_TOUCH, 1}});
        auto drops = BlockActionHandler::blockDrops(fixture.mOwner, &fixture.mLevel, AT, melon, tool);
        require(drops.size() == 1 && Fixture::name(drops[0]) == melon.mName && drops[0].mCount == 1,
                "Silk Touch broke a melon into slices");
        ItemEnchantments::write(tool, {{EnchantmentIds::FORTUNE, 3}});
        for (int trial = 0; trial < 32; ++trial) {
            drops = BlockActionHandler::blockDrops(fixture.mOwner, &fixture.mLevel, AT, melon, tool);
            require(drops.size() == 1 && Fixture::name(drops[0]) == "minecraft:melon_slice" && drops[0].mCount >= 3
                            && drops[0].mCount <= 9,
                    "Fortune melon yield");
        }
        Tag states = Tag::ofCompound();
        states.putInt("growth", 6);
        const Item *meal = VanillaItems::fromIdentifier("minecraft:bone_meal");
        fixture.mLevel.setBlockState(AT.x, AT.y, AT.z, BlockState("minecraft:torchflower_crop", states));
        require(meal->fertilizeBlock(fixture.mOwner, fixture.mLevel, AT)
                        && fixture.mLevel.getBlockState(AT.x, AT.y, AT.z).mName == "minecraft:torchflower",
                "torchflower did not bloom");
        states.putInt("growth", 2);
        states.putByte("upper_block_bit", 0);
        fixture.mLevel.setBlockState(AT.x, AT.y, AT.z, BlockState("minecraft:pitcher_crop", states));
        fixture.mLevel.setBlockState(AT.x, AT.y + 1, AT.z, BlockState("minecraft:stone"));
        require(!meal->fertilizeBlock(fixture.mOwner, fixture.mLevel, AT)
                        && fixture.mLevel.getBlockState(AT.x, AT.y, AT.z).mStates.getInt("growth") == 2,
                "pitcher grew through an obstruction");
        fixture.mLevel.setBlockState(AT.x, AT.y + 1, AT.z, BlockState("minecraft:air"));
        require(meal->fertilizeBlock(fixture.mOwner, fixture.mLevel, AT), "pitcher did not mature");
        const Vector3i above(AT.x, AT.y + 1, AT.z);
        const auto upper = fixture.mLevel.getBlockState(above.x, above.y, above.z);
        require(upper.mName == "minecraft:pitcher_crop" && upper.mStates.getByte("upper_block_bit") != 0
                        && upper.mStates.getInt("growth") == 4,
                "missing mature pitcher upper half");
        BlockActionHandler::destroyBlock(fixture.mOwner, fixture.mLevel, above, upper, false, ItemStack::air());
        require(fixture.mLevel.getBlockState(AT.x, AT.y, AT.z).mName == "minecraft:air",
                "pitcher base survived harvest");
    }

    void plantCrops() {
        Fixture fixture;
        const Vector3i soil(104, 110, 104);
        fixture.mLevel.insertChunk(LevelChunk(6, 6)).setPopulated(true);
        NetworkIdentifier id;
        ServerPlayer &player = fixture.mOwner.getPlayers().try_emplace(id, id, 4001).first->second;
        player.moveToLevel(fixture.mLevel);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(104.5f, 111.f, 102.5f));
        player.getInventoryManager().attach(&player, &fixture.mOwner);
        float click = 0.1f;
        const auto plant = [&](const std::string &item, const std::string &ground, const std::string &crop, int face,
                               const Vector3i &target, bool allowed) {
            fixture.mLevel.setBlockState(target.x, target.y, target.z, BlockState("minecraft:air"));
            fixture.mLevel.setBlockState(soil.x, soil.y, soil.z, BlockState("minecraft:" + ground));
            player.getInventory().setItem(0, fixture.mOwner.createItemStack("minecraft:" + item, 2));
            ItemUseTransaction use;
            use.mBlockPosition = soil;
            use.mBlockFace = face;
            use.mPlayerPosition = player.getPosition();
            use.mItemInHand = player.getInventory().getItemInHand();
            use.mClientInteractPrediction = item == "potato" ? ItemUsePredictedResult::Failure
                                                             : ItemUsePredictedResult::Success;

            click += 0.015f;
            use.mClickPosition = Vector3f(click, 0.5f, 0.5f);
            BlockActionHandler::placeBlock(fixture.mOwner, player, use);
            require(fixture.mLevel.getBlockState(target.x, target.y, target.z).mName
                            == (allowed ? "minecraft:" + crop : "minecraft:air"),
                    item + " placement on " + ground);
            require(player.getInventory().getItemInHand().mCount == (allowed ? 1 : 2), item + " inventory count");
        };
        const Vector3i above(soil.x, soil.y + 1, soil.z), below(soil.x, soil.y - 1, soil.z),
                east(soil.x + 1, soil.y, soil.z);
        for (const auto &[item, crop]:
             std::initializer_list<std::pair<std::string, std::string>>{{"potato", "potatoes"},
                                                                        {"carrot", "carrots"},
                                                                        {"beetroot_seeds", "beetroot"},
                                                                        {"wheat_seeds", "wheat"},
                                                                        {"pumpkin_seeds", "pumpkin_stem"},
                                                                        {"melon_seeds", "melon_stem"},
                                                                        {"torchflower_seeds", "torchflower_crop"},
                                                                        {"pitcher_pod", "pitcher_crop"}}) {
            plant(item, "dirt", crop, 1, above, false);
            plant(item, "farmland", crop, 1, above, true);
        }
        plant("wheat", "farmland", "wheat", 1, above, false);
        plant("beetroot", "farmland", "beetroot", 1, above, false);
        plant("poisonous_potato", "farmland", "potatoes", 1, above, false);
        plant("nether_wart", "dirt", "nether_wart", 1, above, false);
        plant("nether_wart", "soul_sand", "nether_wart", 1, above, true);
        plant("sweet_berries", "grass_block", "sweet_berry_bush", 1, above, true);
        plant("glow_berries", "stone", "cave_vines", 1, above, false);
        plant("glow_berries", "stone", "cave_vines", 0, below, true);
        plant("cocoa_beans", "oak_log", "cocoa", 5, east, false);
        plant("cocoa_beans", "jungle_log", "cocoa", 5, east, true);
    }

    void undyedShulkerBox() {
        Fixture fixture;
        const std::vector<ItemStack> drops = fixture.drops(BlockState(BlockNames::UNDYED_SHULKER_BOX));
        require(drops.size() == 1 && Fixture::name(drops[0]) == BlockNames::UNDYED_SHULKER_BOX,
                "an undyed shulker box did not drop itself");
    }

    void wallSign() {
        Fixture fixture;
        const std::vector<ItemStack> drops = fixture.drops(BlockState("minecraft:spruce_wall_sign"));
        require(drops.size() == 1 && Fixture::name(drops[0]) == "minecraft:spruce_sign",
                "a spruce wall sign did not drop a spruce sign");
    }

    void bedColor() {
        Fixture fixture;
        BedBlockActor &bed = fixture.mLevel.getBlockActors().getOrCreate<BedBlockActor>(AT);
        bed.setColor(14);
        const std::vector<ItemStack> drops = fixture.drops(BlockState(BlockNames::BED));
        require(drops.size() == 1 && Fixture::name(drops[0]) == BlockNames::BED && drops[0].mDamage == 14,
                "a red bed did not drop a red bed");
    }

    void bannerPatterns() {
        Fixture fixture;
        BannerBlockActor &banner = fixture.mLevel.getBlockActors().getOrCreate<BannerBlockActor>(AT);
        Tag saved = Tag::ofCompound();
        saved.putInt("Base", 4);
        saved.putInt("Type", 0);
        Tag pattern = Tag::ofCompound();
        pattern.putString("Pattern", "cre");
        pattern.putInt("Color", 15);
        saved.put("Patterns", Tag::ofList(Tag::Type::Compound, {pattern}));
        banner.loadNbt(saved, fixture.mOwner.getCodecContext());

        const std::vector<ItemStack> drops = fixture.drops(BlockState(BlockNames::WALL_BANNER));
        require(drops.size() == 1 && Fixture::name(drops[0]) == "minecraft:banner",
                "a wall banner did not drop a banner");
        require(drops[0].mDamage == 4, "a blue banner lost its base color");
        const Tag *patterns = drops[0].mTag.isCompound() ? drops[0].mTag.get("Patterns") : nullptr;
        require(patterns != nullptr && patterns->isList() && patterns->getList().size() == 1,
                "a banner lost its patterns");
        require(drops[0].mTag.get("Base") == nullptr, "a banner item kept Base in its tag");
    }

    // a block actor leaves with its block, whatever replaces it; a block of its own type keeps it
    void replacedBlockActors() {
        Fixture fixture;
        Level &level = fixture.mLevel;
        BlockActorStore &actors = level.getBlockActors();
        const Vector3i chest(1, 64, 1), furnace(2, 64, 1), sign(3, 64, 1), barrel(4, 64, 1);

        level.setBlockState(chest.x, chest.y, chest.z, BlockState(BlockNames::CHEST));
        actors.getOrCreate<ChestBlockActor>(chest);
        level.setBlockState(chest.x, chest.y, chest.z, BlockState("minecraft:stone"));
        require(actors.find(chest) == nullptr, "a chest replaced by stone kept its block actor");

        level.setBlockState(furnace.x, furnace.y, furnace.z, BlockState("minecraft:furnace"));
        actors.getOrCreate<FurnaceBlockActor>(furnace);
        level.setBlockState(furnace.x, furnace.y, furnace.z, BlockState("minecraft:lit_furnace"));
        require(actors.find<FurnaceBlockActor>(furnace) != nullptr, "a furnace lost its block actor when it lit");

        // the recipe is found by its input item: raw iron and coal smelt one ingot, a stick as input smelts nothing
        FurnaceBlockActor &smelter = *actors.find<FurnaceBlockActor>(furnace);
        smelter.mInventory.mItems[FurnaceInventory::SLOT_INPUT] = fixture.mOwner.createItemStack("minecraft:raw_iron", 2);
        smelter.mInventory.mItems[FurnaceInventory::SLOT_FUEL] = fixture.mOwner.createItemStack("minecraft:coal", 1);
        for (int tick = 0; tick <= FurnaceInventory::cookDuration(smelter.mKind); ++tick)
            InventoryManager::tickStoredFurnace(fixture.mOwner, smelter);
        const ItemStack &ingot = smelter.mInventory.mItems[FurnaceInventory::SLOT_OUTPUT];
        require(!ingot.isAir() && ingot.mDefinition->getIdentifier() == "minecraft:iron_ingot" && ingot.mCount == 1
                        && smelter.mInventory.mItems[FurnaceInventory::SLOT_INPUT].mCount == 1,
                "raw iron did not smelt into one iron ingot");
        smelter.mInventory.mItems[FurnaceInventory::SLOT_INPUT] = fixture.mOwner.createItemStack("minecraft:stick", 4);
        smelter.mInventory.mItems[FurnaceInventory::SLOT_OUTPUT] = ItemStack::air();
        for (int tick = 0; tick <= FurnaceInventory::cookDuration(smelter.mKind); ++tick)
            InventoryManager::tickStoredFurnace(fixture.mOwner, smelter);
        require(smelter.mInventory.mItems[FurnaceInventory::SLOT_OUTPUT].isAir(), "a stick smelted into something");
        for (ItemStack &item: smelter.mInventory.mItems)
            item = ItemStack::air();

        level.setBlockState(sign.x, sign.y, sign.z, BlockState("minecraft:standing_sign"));
        actors.getOrCreate<SignBlockActor>(sign);
        BlockActionHandler::destroyBlock(fixture.mOwner, level, sign, BlockState("minecraft:standing_sign"), false,
                                         ItemStack::air());
        require(actors.find(sign) == nullptr, "a broken sign kept its block actor");

        level.setBlockState(barrel.x, barrel.y, barrel.z, BlockState("minecraft:barrel"));
        actors.getOrCreate<BarrelBlockActor>(barrel);
        level.setBlockState(barrel.x, barrel.y, barrel.z, BlockState("minecraft:hopper"));
        require(actors.find(barrel) == nullptr, "a hopper took over a barrel's block actor");
    }
}

int main() {
    try {
        cropDrops();
        plantCrops();
        cropToolsAndFlowers();
        undyedShulkerBox();
        wallSign();
        bedColor();
        bannerPatterns();
        replacedBlockActors();
        std::cout << "block drop checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
