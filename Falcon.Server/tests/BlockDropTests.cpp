#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/BannerBlockActor.h"
#include "World/Level/Block/Actor/BedBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/BlockNames.h"
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
