#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/SignEditing.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState standing(int direction) {
        BlockState state("minecraft:standing_sign");
        state.mStates.putInt("ground_sign_direction", direction);
        return state;
    }

    BlockState wall(int facing) {
        BlockState state("minecraft:wall_sign");
        state.mStates.putInt("facing_direction", facing);
        return state;
    }

    void rotations() {
        require(std::fabs(SignEditing::rotationOf(standing(0))) < 0.001f, "a standing sign facing south is not at 0");
        require(std::fabs(SignEditing::rotationOf(standing(4)) - 90.0f) < 0.001f,
                "a standing sign facing west is not at 90");
        require(std::fabs(SignEditing::rotationOf(wall(2)) + 180.0f) < 0.001f, "a north wall sign is not at -180");
        require(std::fabs(SignEditing::rotationOf(wall(3))) < 0.001f, "a south wall sign is not at 0");
        require(std::fabs(SignEditing::rotationOf(wall(4)) - 90.0f) < 0.001f, "a west wall sign is not at 90");
        require(std::fabs(SignEditing::rotationOf(wall(5)) + 90.0f) < 0.001f, "an east wall sign is not at -90");
    }

    void sides() {
        const Vector3i at(0, 64, 0);
        require(SignEditing::isFrontFacing(standing(0), at, 0.5f, 3.0f), "south of a south sign is not its front");
        require(!SignEditing::isFrontFacing(standing(0), at, 0.5f, -3.0f), "north of a south sign is not its back");
        require(SignEditing::isFrontFacing(wall(5), at, 3.0f, 0.5f), "east of an east wall sign is not its front");
        require(!SignEditing::isFrontFacing(wall(4), at, 3.0f, 0.5f), "east of a west wall sign is not its back");
    }

    void dyeColors() {
        require(SignEditing::dyeTextColor(0) == (int32_t) 0xFF000000, "black dye does not give pure black");
        require(SignEditing::dyeTextColor(1) == (int32_t) 0xFFB02E26, "red dye text color is wrong");
        require(SignEditing::dyeTextColor(15) == (int32_t) 0xFFF0F0F0, "white dye text color is wrong");
    }

    void updateNeedsEditor() {
        ServerNetworkHandler owner("Signs", "", 1);
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;

        SignBlockActor sign;
        Tag update = Tag::ofCompound();
        Tag front = Tag::ofCompound();
        front.putString("Text", "hello");
        update.put("FrontText", front);

        require(!SignEditing::applyUpdate(owner, player, sign, update), "a sign took text with no editor open");
        require(sign.getSide(true).getString("Text", "").empty(), "a refused update changed the text");

        sign.setEditor(player.getUniqueId());
        require(!SignEditing::applyUpdate(owner, player, sign, update),
                "a sign took text from an editor no longer in the game");
        require(sign.getEditor() == -1, "a stale editor lock was kept");
    }
}

int main() {
    try {
        rotations();
        sides();
        dyeColors();
        updateNeedsEditor();
        std::cout << "sign checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
