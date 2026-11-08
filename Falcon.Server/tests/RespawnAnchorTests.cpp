#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockLightProperties.h"
#include "World/Level/Block/RespawnAnchorBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace {
    const Vector3i ANCHOR(8, 64, 8);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState anchor(int charge) {
        BlockState state = VanillaBlocks::RESPAWN_ANCHOR().toBlockState();
        state.mStates.putInt("respawn_anchor_charge", charge);
        return state;
    }

    ItemStack stack(const char *identifier, int count) {
        ItemStack item;
        item.mDefinition = std::make_shared<ItemDefinition>(identifier, 1, false, Tag());
        item.mCount = count;
        return item;
    }

    // an anchor on stone in a cleared column of the chosen dimension, the player beside it
    struct AnchorFixture {
        ServerNetworkHandler mOwner{"Respawn anchor", "", 1};
        World &mWorld = mOwner.getWorlds().getDefault();
        Level &mLevel;
        ServerPlayer mPlayer{NetworkIdentifier{}, 100};

        explicit AnchorFixture(DimensionType dimension) : mLevel(mWorld.getLevel(dimension)) {
            mLevel.getChunk(0, 0);
            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    for (int y = 60; y <= 72; ++y)
                        mLevel.setBlockState(x, y, z, BlockState("minecraft:air"));
                    mLevel.setBlockState(x, 63, z, VanillaBlocks::STONE().toBlockState());
                }
            }

            mPlayer.moveToLevel(mLevel);
            mPlayer.setPosition(Vector3f(10.3f, 64.0f, 8.7f));
        }

        void place(int charge) {
            mLevel.setBlockState(ANCHOR.x, ANCHOR.y, ANCHOR.z, anchor(charge));
        }

        int charge() {
            return RespawnAnchorBlock::getCharge(mLevel.getBlockState(ANCHOR.x, ANCHOR.y, ANCHOR.z));
        }

        std::string at(int x, int y, int z) {
            return mLevel.getBlockState(x, y, z).mName;
        }

        bool use() {
            const BlockState state = mLevel.getBlockState(ANCHOR.x, ANCHOR.y, ANCHOR.z);
            const Block *block = VanillaBlocks::fromIdentifier(state.mName);
            require(block != nullptr, "the anchor has no block class");
            return block->onInteract(mOwner, mPlayer, ANCHOR, state);
        }

        void hold(const char *identifier, int count) {
            mPlayer.getInventory().setItemInHand(stack(identifier, count));
        }

        int held() {
            return mPlayer.getInventory().getItemInHand().isAir() ? 0 : mPlayer.getInventory().getItemInHand().mCount;
        }
    };

    void recharge() {
        AnchorFixture fixture(DimensionType::Nether);
        require(VanillaBlocks::getAs<RespawnAnchorBlock>("minecraft:respawn_anchor") != nullptr,
                "the respawn anchor is not a RespawnAnchorBlock");

        fixture.place(0);
        fixture.hold("minecraft:glowstone", 6);
        for (int expected = 1; expected <= RespawnAnchorBlock::MAX_CHARGE; ++expected) {
            require(fixture.use(), "charging with glowstone was not handled");
            require(fixture.charge() == expected, "glowstone did not add one charge");
        }
        require(fixture.held() == 2, "each charge did not take one glowstone");

        require(fixture.use() && fixture.charge() == RespawnAnchorBlock::MAX_CHARGE && fixture.held() == 2,
                "a full anchor took more glowstone");

        AnchorFixture creative(DimensionType::Nether);
        creative.place(0);
        creative.mPlayer.setGameType((int32_t) GameType::Creative);
        creative.hold("minecraft:glowstone", 1);
        require(creative.use() && creative.charge() == 1 && creative.held() == 1, "creative used up the glowstone");

        AnchorFixture other(DimensionType::Nether);
        other.place(0);
        other.hold("minecraft:glowstone_dust", 4);
        require(!other.use() && other.charge() == 0, "glowstone dust charged the anchor");

        AnchorFixture empty(DimensionType::Nether);
        empty.place(0);
        require(!empty.use() && !empty.mPlayer.hasSpawnPoint(), "an empty anchor did something by hand");
    }

    void setSpawn() {
        AnchorFixture fixture(DimensionType::Nether);
        fixture.place(1);
        require(fixture.use(), "setting the spawn was not handled");
        require(fixture.mPlayer.hasSpawnPoint() && fixture.mPlayer.getSpawnPoint() == ANCHOR
                && fixture.mPlayer.getSpawnDimension() == DimensionType::Nether,
                "the anchor did not become the spawn point");
        require(fixture.mPlayer.getSpawnStandPosition() == Vector3i(10, 64, 8),
                "the player position was not kept with the spawn point");
        require(fixture.charge() == 1, "setting the spawn spent a charge");

        fixture.mPlayer.setPosition(Vector3f(5.5f, 64.0f, 5.5f));
        require(!fixture.use() && fixture.mPlayer.getSpawnStandPosition() == Vector3i(10, 64, 8),
                "the anchor set the spawn point it already held");

        fixture.hold("minecraft:glowstone", 1);
        fixture.place(RespawnAnchorBlock::MAX_CHARGE);
        require(fixture.use() && fixture.held() == 1, "glowstone on a full anchor holding the spawn was placed");
    }

    void explosion() {
        {
            AnchorFixture fixture(DimensionType::Overworld);
            fixture.place(1);
            fixture.mLevel.setBlockState(ANCHOR.x + 1, ANCHOR.y, ANCHOR.z, VanillaBlocks::STONE().toBlockState());
            require(fixture.use(), "the anchor outside the Nether was not handled");
            require(fixture.at(ANCHOR.x, ANCHOR.y, ANCHOR.z) == "minecraft:air", "the anchor did not blow up");
            require(fixture.at(ANCHOR.x + 1, ANCHOR.y, ANCHOR.z) == "minecraft:air",
                    "the blast did not break the stone beside it");
            require(!fixture.mPlayer.hasSpawnPoint(), "the anchor outside the Nether set the spawn");
        }
        {
            AnchorFixture fixture(DimensionType::Overworld);
            fixture.mLevel.getGameRules().setFromString("respawnblocksexplode", "false");
            fixture.place(1);
            require(fixture.use() && fixture.charge() == 1, "the anchor exploded with respawnblocksexplode off");
        }
        {
            AnchorFixture fixture(DimensionType::Overworld);
            fixture.place(0);
            require(!fixture.use() && fixture.charge() == 0, "an empty anchor exploded");
        }
        {
            AnchorFixture fixture(DimensionType::TheEnd);
            fixture.place(2);
            fixture.mLevel.setBlockState(ANCHOR.x + 1, ANCHOR.y, ANCHOR.z, VanillaBlocks::STONE().toBlockState());
            fixture.mLevel.setBlockState(ANCHOR.x, ANCHOR.y + 1, ANCHOR.z, BlockState("minecraft:water"));
            fixture.mLevel.setBlockState(ANCHOR.x, ANCHOR.y, ANCHOR.z - 1, BlockState("minecraft:flowing_water"));
            require(RespawnAnchorBlock::isSurroundedByWater(fixture.mLevel, ANCHOR), "two water sides not seen");
            require(fixture.use() && fixture.at(ANCHOR.x, ANCHOR.y, ANCHOR.z) == "minecraft:air",
                    "the anchor in water was not removed");
            require(fixture.at(ANCHOR.x + 1, ANCHOR.y, ANCHOR.z) == "minecraft:stone"
                    && fixture.at(ANCHOR.x, ANCHOR.y - 1, ANCHOR.z) == "minecraft:stone",
                    "the blast in water broke blocks");
        }
        {
            AnchorFixture fixture(DimensionType::Overworld);
            fixture.mLevel.setBlockState(ANCHOR.x, ANCHOR.y + 1, ANCHOR.z, BlockState("minecraft:water"));
            require(!RespawnAnchorBlock::isSurroundedByWater(fixture.mLevel, ANCHOR), "one water side shielded");
        }
    }

    void standUpPosition() {
        AnchorFixture fixture(DimensionType::Nether);
        const Vector3i stand(10, 64, 8);
        require(RespawnAnchorBlock::findStandUpPosition(fixture.mLevel, stand) == std::optional<Vector3i>(stand),
                "a free spot on stone was not kept");

        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z)
                fixture.mLevel.setBlockState(x, 63, z, BlockState("minecraft:air"));
        }
        require(!RespawnAnchorBlock::findStandUpPosition(fixture.mLevel, stand).has_value(),
                "a spot without floor was found");

        fixture.mLevel.setBlockState(12, 63, 7, VanillaBlocks::STONE().toBlockState());
        fixture.mLevel.setBlockState(9, 63, 10, VanillaBlocks::STONE().toBlockState());
        require(RespawnAnchorBlock::findStandUpPosition(fixture.mLevel, stand) == std::optional<Vector3i>(Vector3i(9, 64, 10)),
                "the search did not run x first, then z");

        fixture.mLevel.setBlockState(9, 65, 10, VanillaBlocks::STONE().toBlockState());
        require(RespawnAnchorBlock::findStandUpPosition(fixture.mLevel, stand) == std::optional<Vector3i>(Vector3i(12, 64, 7)),
                "a spot without head room was taken");

        fixture.mLevel.setBlockState(13, 63, 8, VanillaBlocks::STONE().toBlockState());
        fixture.mLevel.setBlockState(12, 63, 7, BlockState("minecraft:air"));
        require(!RespawnAnchorBlock::findStandUpPosition(fixture.mLevel, stand).has_value(),
                "the search went past x + 2");
    }

    void respawnCharge() {
        AnchorFixture fixture(DimensionType::Nether);
        fixture.place(2);
        const std::optional<Vector3f> spawn = RespawnAnchorBlock::respawn(fixture.mOwner, fixture.mLevel, ANCHOR,
                                                                          Vector3i(10, 64, 8));
        require(spawn.has_value() && spawn->x == 10.5f && spawn->y == 64.0f && spawn->z == 8.5f,
                "the respawn was not centered on the free spot");
        require(fixture.charge() == 1, "the respawn did not spend one charge");

        require(RespawnAnchorBlock::respawn(fixture.mOwner, fixture.mLevel, ANCHOR, Vector3i(10, 64, 8)).has_value()
                && fixture.charge() == 0, "the last charge was not spent");
        require(!RespawnAnchorBlock::respawn(fixture.mOwner, fixture.mLevel, ANCHOR, Vector3i(10, 64, 8)).has_value(),
                "an empty anchor respawned the player");

        fixture.place(3);
        fixture.mLevel.setBlockState(10, 64, 8, VanillaBlocks::STONE().toBlockState());
        fixture.mLevel.setBlockState(10, 65, 8, VanillaBlocks::STONE().toBlockState());
        for (int x = 9; x <= 12; ++x) {
            for (int z = 7; z <= 10; ++z)
                fixture.mLevel.setBlockState(x, 64, z, VanillaBlocks::STONE().toBlockState());
        }
        require(!RespawnAnchorBlock::respawn(fixture.mOwner, fixture.mLevel, ANCHOR, Vector3i(10, 64, 8)).has_value()
                && fixture.charge() == 3, "a blocked anchor spent a charge");

        fixture.mLevel.setBlockState(ANCHOR.x, ANCHOR.y, ANCHOR.z, BlockState("minecraft:air"));
        require(!RespawnAnchorBlock::isChargedAnchor(fixture.mLevel, ANCHOR), "a broken anchor still counts");
    }

    void light() {
        const int expected[] = {0, 3, 7, 11, 15};
        for (int charge = 0; charge <= RespawnAnchorBlock::MAX_CHARGE; ++charge) {
            const BlockState state = anchor(charge);
            require(BlockLightProperties::lightLevel(BlockLightProperties::packed(state)) == expected[charge],
                    "wrong light for charge " + std::to_string(charge));
        }
    }

    void persistence() {
        ServerNetworkHandler owner{"Respawn anchor save", "", 1};
        ServerPlayer player{NetworkIdentifier{}, 101};
        player.setSpawnPoint(ANCHOR, Vector3i(10, 64, 8), DimensionType::Nether, "world");
        const Tag saved = player.saveNbt("world");
        require(saved.getInt("SpawnBlockPositionX", 0) == ANCHOR.x && saved.getInt("SpawnDimension", 0) == 1
                && saved.getInt("SpawnX", 0) == 10, "the spawn point was not saved like the game");

        ServerPlayer loaded{NetworkIdentifier{}, 102};
        loaded.loadNbt(saved, owner.getCodecContext());
        require(loaded.hasSpawnPoint() && loaded.getSpawnPoint() == ANCHOR
                && loaded.getSpawnStandPosition() == Vector3i(10, 64, 8)
                && loaded.getSpawnDimension() == DimensionType::Nether, "the spawn point did not load back");

        Tag legacy = saved;
        legacy.remove("SpawnBlockPositionX");
        legacy.remove("SpawnDimension");
        ServerPlayer old{NetworkIdentifier{}, 103};
        old.loadNbt(legacy, owner.getCodecContext());
        require(old.hasSpawnPoint() && old.getSpawnPoint() == Vector3i(10, 64, 8)
                && old.getSpawnDimension() == DimensionType::Overworld, "an older bed spawn did not load");

        Tag unset = saved;
        unset.putInt("SpawnBlockPositionX", std::numeric_limits<int32_t>::min());
        unset.putInt("SpawnDimension", 3);
        ServerPlayer none{NetworkIdentifier{}, 104};
        none.loadNbt(unset, owner.getCodecContext());
        require(!none.hasSpawnPoint(), "the game's unset spawn point loaded as a spawn");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        recharge();
        setSpawn();
        explosion();
        standUpPosition();
        respawnCharge();
        light();
        persistence();
        std::cout << "respawn anchor checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
