#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <cmath>
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

    void checkShapes() {
        const ServerPlayer player(NetworkIdentifier{}, 100);
        const ItemActor item(101, ItemStack::air());
        const std::unique_ptr<ServerActor> arrow = create("minecraft:arrow");
        const std::unique_ptr<ServerActor> snowball = create("minecraft:snowball");
        const std::unique_ptr<ServerActor> firework = create("minecraft:fireworks_rocket");
        const std::unique_ptr<ServerActor> rabbit = create("minecraft:rabbit");
        const std::unique_ptr<ServerActor> polarBear = create("minecraft:polar_bear");
        require(arrow->isProjectile() && snowball->isProjectile() && firework->isProjectile(),
                "arrows, snowballs and fireworks are projectiles");

        const int32_t lowered[8] = {-1, 1, 2, 3, 4, 5, 6, 7};
        const int32_t look[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        checkRow("no actor", nullptr, lowered);
        checkRow("player", &player, lowered);
        checkRow("item", &item, look);
        checkRow("arrow", arrow.get(), look);
        checkRow("snowball", snowball.get(), look);
        checkRow("firework", firework.get(), lowered);
        checkRow("rabbit", rabbit.get(), lowered);
        checkRow("polar bear", polarBear.get(), lowered);
    }

    bool closeTo(float value, float expected) {
        return std::fabs(value - expected) < 1.0e-4f;
    }

    ItemActor *settle(ServerNetworkHandler &owner, Level &level, const Vector3f &position) {
        ItemActor *item = owner.dropItem(level, position, owner.createItemStack("minecraft:diamond", 1),
                                         Vector3f(0.0f, 0.0f, 0.0f), 0);
        require(item != nullptr, "could not drop an item");
        for (int32_t tick = 0; tick < 100 && !item->isOnGround(); tick++)
            ItemActorHandler::tickItemActors(owner);
        require(item->isOnGround(), "the item never landed");
        return item;
    }

    void checkItemLanding() {
        ServerNetworkHandler owner("Snow layer", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);

        const int32_t groundY = 60;
        level.setBlockState(1, groundY, 1, BlockState("minecraft:stone"));
        level.setBlockState(1, groundY + 1, 1, snow(0));
        level.setBlockState(3, groundY, 3, BlockState("minecraft:stone"));

        ItemActor *onSnow = settle(owner, level, Vector3f(1.5f, (float) groundY + 3.0f, 1.5f));
        require(closeTo(onSnow->getPosition().y, (float) groundY + 1.0f + 1.0f / 8.0f),
                "an item lands on a single snow layer at its look, got " + std::to_string(onSnow->getPosition().y));

        level.setBlockState(1, groundY + 1, 1, snow(3));
        ItemActorHandler::tickItemActors(owner);
        require(closeTo(onSnow->getPosition().y, (float) groundY + 1.0f + 4.0f / 8.0f),
                "a layer grown under a resting item lifts it, got " + std::to_string(onSnow->getPosition().y));

        ItemActor *onStone = settle(owner, level, Vector3f(3.5f, (float) groundY + 3.0f, 3.5f));
        require(closeTo(onStone->getPosition().y, (float) groundY + 1.0f),
                "an item still lands on top of a full block, got " + std::to_string(onStone->getPosition().y));
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        checkShapes();
        checkItemLanding();
    } catch (const std::exception &exception) {
        std::cout << "Snow layer collision check failed: " << exception.what() << '\n';
        return 1;
    }

    std::cout << "Snow layer collision checks passed" << std::endl;
    return 0;
}
