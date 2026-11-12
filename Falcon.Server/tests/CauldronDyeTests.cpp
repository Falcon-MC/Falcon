#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/CauldronBlockActor.h"
#include "World/Level/Block/CauldronBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const Vector3i CAULDRON(8, 64, 8);
    // blue dye mixed into plain water, as the script addDye mixes it
    int32_t blue() {
        CauldronBlockActor mixed;
        mixed.addDye(CauldronBlockActor::dyeColorOf("minecraft:blue_dye"));
        return mixed.getColor();
    }

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct CauldronFixture {
        ServerNetworkHandler mOwner{"Cauldron dye", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();
        ServerPlayer mPlayer{NetworkIdentifier{}, 100};

        explicit CauldronFixture(int fill) {
            mLevel.getChunk(0, 0);
            mLevel.setBlockState(CAULDRON.x, CAULDRON.y, CAULDRON.z,
                                 CauldronBlock::makeState(CauldronBlock::Liquid::Water, fill));
            mPlayer.moveToLevel(mLevel);
            mPlayer.setPosition(Vector3f(10.3f, 64.0f, 8.7f));
        }

        bool use() {
            const BlockState state = mLevel.getBlockState(CAULDRON.x, CAULDRON.y, CAULDRON.z);
            const Block *block = VanillaBlocks::fromIdentifier(state.mName);
            require(block != nullptr, "the cauldron has no block class");
            return block->onInteract(mOwner, mPlayer, CAULDRON, state);
        }

        void hold(const char *identifier, int count) {
            mPlayer.getInventory().setItemInHand(mOwner.createItemStack(identifier, count));
        }

        const ItemStack &held() {
            return mPlayer.getInventory().getItemInHand();
        }

        int fill() {
            return CauldronBlock(mLevel.getBlockState(CAULDRON.x, CAULDRON.y, CAULDRON.z)).getFillLevel();
        }

        CauldronBlockActor &actor() {
            return mLevel.getBlockActors().getOrCreate<CauldronBlockActor>(CAULDRON);
        }
    };

    void dyeWater() {
        const int32_t BLUE = blue();
        CauldronFixture fixture(3);
        fixture.hold("minecraft:blue_dye", 2);
        require(fixture.use(), "a dye on water was not handled");
        require(fixture.actor().hasColor() && fixture.actor().getColor() == BLUE, "the dye did not color the water");
        require(fixture.held().mCount == 1, "dyeing did not take one dye");
        require(fixture.fill() == 3, "dyeing changed the water level");

        CauldronFixture creative(3);
        creative.mPlayer.setGameType((int32_t) GameType::Creative);
        creative.hold("minecraft:blue_dye", 1);
        require(creative.use() && creative.held().mCount == 1, "creative used up the dye");

        CauldronFixture potion(3);
        Tag data = potion.actor().saveNbt();
        data.putShort("PotionId", 5);
        potion.actor().loadNbt(data, potion.mOwner.getCodecContext());
        potion.hold("minecraft:blue_dye", 1);
        require(!potion.use() && !potion.actor().hasColor(), "a dye colored a potion");

        CauldronFixture stone(3);
        stone.hold("minecraft:stone", 1);
        require(!stone.use(), "stone did something to the water");
    }

    void dyeArmor() {
        const int32_t BLUE = blue();
        CauldronFixture fixture(2);
        fixture.hold("minecraft:blue_dye", 1);
        require(fixture.use(), "the dye was refused");
        fixture.hold("minecraft:leather_helmet", 1);
        require(fixture.use(), "a leather helmet in dyed water was not handled");
        require(fixture.held().mTag.getInt("customColor", 0) == BLUE, "the helmet did not take the water color");
        require(fixture.fill() == 1, "dyeing the helmet did not use a level");

        // washing in plain water takes the color off and a level, then the last level empties the cauldron
        CauldronFixture wash(1);
        ItemStack dyed = wash.mOwner.createItemStack("minecraft:leather_boots", 1);
        dyed.mTag = Tag::ofCompound();
        dyed.mTag.putInt("customColor", BLUE);
        wash.mPlayer.getInventory().setItemInHand(dyed);
        require(wash.use(), "washing the boots was not handled");
        require(!wash.held().mTag.contains("customColor"), "the wash left the color");
        require(wash.fill() == 0, "the wash did not use the last level");

        CauldronFixture clean(3);
        clean.hold("minecraft:leather_boots", 1);
        require(!clean.use() && clean.fill() == 3, "plain water washed boots with no color");

        CauldronFixture iron(3);
        iron.hold("minecraft:iron_helmet", 1);
        require(!iron.use(), "an iron helmet took a color");
    }
}

int main() {
    try {
        dyeWater();
        dyeArmor();
    } catch (const std::exception &error) {
        std::clog << "Cauldron dye regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
