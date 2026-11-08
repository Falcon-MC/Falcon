#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/BaseGameVersion.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState snow(int32_t height) {
        BlockState state("minecraft:snow_layer");
        state.mStates.putInt("height", height);
        state.mStates.putByte("covered_bit", 0);
        return state;
    }

    // collision top in eighths, -1 when the shape lets the actor through
    int32_t topEighths(int32_t height, const Actor *actor) {
        const BlockState state = snow(height);
        if (!BlockShape::hasCollision(state, actor))
            return -1;

        return (int32_t) (BlockShape::getRelativeShape(state, actor).mMaxY * 8.0f + 0.5f);
    }

    void checkRow(const char *name, const Actor *actor, const int32_t (&expected)[8]) {
        for (int32_t height = 0; height < 8; height++) {
            const int32_t top = topEighths(height, actor);
            require(top == expected[height], std::string(name) + " height " + std::to_string(height) + ": top "
                    + std::to_string(top) + ", expected " + std::to_string(expected[height]));
        }
    }

    std::unique_ptr<ServerActor> create(const char *identifier) {
        std::unique_ptr<ServerActor> actor = ActorClassRegistry::create(100, identifier);
        require(actor != nullptr, std::string("could not create ") + identifier);
        return actor;
    }

    void checkModern() {
        BaseGameVersion::setCurrent(BaseGameVersion::parse("1.26.60"));

        const ServerPlayer player(NetworkIdentifier{}, 100);
        const ItemActor item(101, ItemStack::air());
        const std::unique_ptr<ServerActor> arrow = create("minecraft:arrow");
        const std::unique_ptr<ServerActor> painting = create("minecraft:painting");
        const std::unique_ptr<ServerActor> rabbit = create("minecraft:rabbit");
        const std::unique_ptr<ServerActor> polarBear = create("minecraft:polar_bear");
        require(arrow->isProjectile(), "an arrow is a projectile");

        const int32_t lowered[8] = {-1, 1, 2, 3, 4, 5, 6, 7};
        const int32_t look[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        const int32_t none[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
        checkRow("no actor", nullptr, lowered);
        checkRow("player", &player, lowered);
        checkRow("item", &item, look);
        checkRow("arrow", arrow.get(), look);
        checkRow("painting", painting.get(), none);
        checkRow("rabbit", rabbit.get(), lowered);
        checkRow("polar bear", polarBear.get(), lowered);
    }

    void checkLegacy() {
        BaseGameVersion::setCurrent(BaseGameVersion::parse("1.20.10"));

        const ServerPlayer player(NetworkIdentifier{}, 100);
        const ItemActor item(101, ItemStack::air());
        const std::unique_ptr<ServerActor> arrow = create("minecraft:arrow");
        const std::unique_ptr<ServerActor> painting = create("minecraft:painting");
        const std::unique_ptr<ServerActor> rabbit = create("minecraft:rabbit");

        const int32_t steps[8] = {-1, -1, -1, 4, 4, 4, 4, 8};
        const int32_t lowered[8] = {-1, 1, 2, 3, 4, 5, 6, 7};
        const int32_t look[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        const int32_t none[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
        checkRow("legacy no actor", nullptr, steps);
        checkRow("legacy player", &player, steps);
        checkRow("legacy item", &item, look);
        checkRow("legacy arrow", arrow.get(), look);
        checkRow("legacy painting", painting.get(), none);
        checkRow("legacy rabbit", rabbit.get(), lowered);
    }

    void checkVersions() {
        require(!BaseGameVersion::parse("*").isBefore(1, 20, 20), "any version takes the new shapes");
        require(!BaseGameVersion::parse("1.20.20").isBefore(1, 20, 20), "1.20.20 takes the new shapes");
        require(BaseGameVersion::parse("1.20.10").isBefore(1, 20, 20), "1.20.10 keeps the old shapes");
        require(BaseGameVersion::parse("1.19.80").isBefore(1, 20, 20), "1.19.80 keeps the old shapes");
        require(!BaseGameVersion::parse("1.21").isBefore(1, 20, 20), "1.21 takes the new shapes");
        require(BaseGameVersion::parse("1.20").isBefore(1, 20, 20), "1.20 reads as 1.20.0");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        checkVersions();
        checkModern();
        checkLegacy();
        BaseGameVersion::setCurrent(BaseGameVersion());
    } catch (const std::exception &exception) {
        std::cout << "Snow layer collision check failed: " << exception.what() << '\n';
        return 1;
    }

    std::cout << "Snow layer collision checks passed" << std::endl;
    return 0;
}
