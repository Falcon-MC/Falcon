#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "World/Actor/Player/ServerPlayer.h"
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

    void carrots() {
        Fixture fixture;
        Tag states = Tag::ofCompound();
        for (int age = 0; age <= 7; ++age) {
            states.putInt("growth", age);
            const auto drops = fixture.drops(BlockState("minecraft:carrots", states));
            require(drops.size() == 1 && Fixture::name(drops[0]) == "minecraft:carrot", "crop dropped a block item");
            if (age == 7)
                require(drops[0].mCount >= 1 && drops[0].mCount <= 4, "mature carrot yield out of range");
            else
                require(drops[0].mCount == 1, "immature plant dropped extra carrots");
        }

        const Vector3i soil(104, 110, 104);
        fixture.mLevel.insertChunk(LevelChunk(6, 6)).setPopulated(true);
        NetworkIdentifier id;
        ServerPlayer &player = fixture.mOwner.getPlayers().try_emplace(id, id, 4001).first->second;
        player.moveToLevel(fixture.mLevel);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(104.5f, 111.f, 102.5f));
        player.getInventoryManager().attach(&player, &fixture.mOwner);
        player.getInventory().setItem(0, fixture.mOwner.createItemStack("minecraft:carrot", 2));
        ItemUseTransaction use;
        use.mBlockPosition = soil;
        use.mBlockFace = 1;
        use.mClickPosition = Vector3f(0.5f, 1.f, 0.5f);
        use.mPlayerPosition = player.getPosition();
        use.mItemInHand = player.getInventory().getItemInHand();
        use.mClientInteractPrediction = ItemUsePredictedResult::Success;
        fixture.mLevel.setBlockState(soil.x, soil.y, soil.z, BlockState("minecraft:dirt"));
        BlockActionHandler::placeBlock(fixture.mOwner, player, use);
        require(fixture.mLevel.getBlockState(soil.x, soil.y + 1, soil.z).mName == "minecraft:air"
                        && player.getInventory().getItemInHand().mCount == 2,
                "carrot planted on dirt");
        use.mClickPosition = Vector3f(0.6f, 1.f, 0.5f);
        fixture.mLevel.setBlockState(soil.x, soil.y, soil.z, BlockState("minecraft:farmland"));
        BlockActionHandler::placeBlock(fixture.mOwner, player, use);
        const auto planted = fixture.mLevel.getBlockState(soil.x, soil.y + 1, soil.z);
        require(planted.mName == "minecraft:carrots" && planted.mStates.getInt("growth") == 0,
                "carrot did not plant a fresh crop on farmland");
        require(player.getInventory().getItemInHand().mCount == 1, "planting did not consume one carrot");
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
        carrots();
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
