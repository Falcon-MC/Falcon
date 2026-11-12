#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BedBlock.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    // direction 0: the head lies south of the foot
    const Vector3i HEAD(8, 64, 8);
    const Vector3i FOOT(8, 64, 7);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState bedPart(bool head) {
        BlockState state(BlockNames::BED);
        state.mStates.putInt("direction", 0);
        state.mStates.putByte("head_piece_bit", head ? 1 : 0);
        state.mStates.putByte("occupied_bit", 0);
        return state;
    }

    // a bed on stone in a cleared area, the player beside it
    struct BedFixture {
        ServerNetworkHandler mOwner{"Bed", "", 1};
        World &mWorld = mOwner.getWorlds().getDefault();
        Level &mLevel = mWorld.getOverworld();
        ServerPlayer mPlayer{NetworkIdentifier{}, 100};

        BedFixture() {
            mLevel.getChunk(0, 0);
            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    for (int y = 60; y <= 72; ++y)
                        mLevel.setBlockState(x, y, z, BlockState(BlockNames::AIR));
                    mLevel.setBlockState(x, 63, z, VanillaBlocks::STONE().toBlockState());
                }
            }
            mLevel.setBlockState(HEAD.x, HEAD.y, HEAD.z, bedPart(true));
            mLevel.setBlockState(FOOT.x, FOOT.y, FOOT.z, bedPart(false));

            mPlayer.moveToLevel(mLevel);
            mPlayer.setPosition(Vector3f(7.5f, 64.0f, 8.5f));
        }

        Vector3i standUp(const Vector3f &previous) {
            Vector3i found;
            require(BedBlock::findStandUpPosition(mLevel, HEAD, previous, found), "no stand up position was found");
            return found;
        }
    };

    void standUpPosition() {
        BedFixture fixture;
        // rotation 2 turns the first offset (1, 0, 0) to the west of the head
        require(fixture.standUp(Vector3f(7.0f, 65.62f, 8.0f)) == Vector3i(7, 64, 8), "the first offset was not kept");
        // lying down from the east takes the mirrored table
        require(fixture.standUp(Vector3f(9.0f, 65.62f, 8.0f)) == Vector3i(9, 64, 8),
                "the other side did not mirror the offsets");
        Vector3i fromFoot;
        require(BedBlock::findStandUpPosition(fixture.mLevel, FOOT, Vector3f(7.0f, 65.62f, 8.0f), fromFoot)
                        && fromFoot == Vector3i(7, 64, 8),
                "the search did not start from the head");

        // magma below is only a fallback, the next safe offset wins
        fixture.mLevel.setBlockState(7, 63, 8, BlockState(BlockNames::MAGMA));
        require(fixture.standUp(Vector3f(7.0f, 65.62f, 8.0f)) == Vector3i(7, 64, 7), "magma was not avoided");

        // a solid block or one without floor is skipped
        fixture.mLevel.setBlockState(7, 63, 8, VanillaBlocks::STONE().toBlockState());
        fixture.mLevel.setBlockState(7, 64, 8, VanillaBlocks::STONE().toBlockState());
        require(fixture.standUp(Vector3f(7.0f, 65.62f, 8.0f)) == Vector3i(7, 64, 7), "a solid block was taken");
    }

    void wakeUp() {
        BedFixture fixture;
        require(fixture.mOwner.sleepOn(fixture.mPlayer, HEAD), "the player could not lie down");
        fixture.mPlayer.setBedEnterPosition(Vector3f(7.5f, 65.62f, 8.5f));
        fixture.mOwner.stopSleep(fixture.mPlayer);
        const Vector3f position = fixture.mPlayer.getPosition();
        require(std::fabs(position.x - 7.5f) < 0.001f && std::fabs(position.y - 64.1f) < 0.001f
                        && std::fabs(position.z - 8.5f) < 0.001f,
                "the player did not stand up beside the bed");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        standUpPosition();
        wakeUp();
        std::cout << "bed checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
