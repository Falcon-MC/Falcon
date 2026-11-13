#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"
#include "World/Level/Vibration/GameEventListener.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const Vector3i AT(8, 100, 8);
    const int32_t FACING_EAST = 5;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // every game event within its range, where it was posted
    class RecordingBlockActor final : public BlockActor, public GameEventListener {
    public:
        explicit RecordingBlockActor(const Vector3i &position) {
            setPosition(position);
        }

        const char *getBlockActorId() const override {
            return "DispenserProbe";
        }

        Tag saveNbt() const override {
            return Tag::ofCompound();
        }

        void loadNbt(const Tag &data, const PacketCodecContext &context) override {
            (void) data;
            (void) context;
        }

        GameEventListener *getGameEventListener() override {
            return this;
        }

        void handleGameEvent(Level &level, const GameEvent &event, const GameEventContext &context) override {
            (void) level;
            mEvents.push_back(&event);
            mPositions.push_back(context.mPosition);
        }

        bool getPosition(Level &level, Vector3f &position) const override {
            (void) level;
            const Vector3i &block = BlockActor::getPosition();
            position = Vector3f((float) block.x + 0.5f, (float) block.y + 0.5f, (float) block.z + 0.5f);
            return true;
        }

        uint32_t getRange() const override {
            return 8;
        }

        std::vector<const GameEvent *> mEvents;
        std::vector<Vector3f> mPositions;
    };

    struct Fixture {
        ServerNetworkHandler mOwner{"Dispenser", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();
        BlockState mState;
        DispenserBlockActor *mActor = nullptr;

        Fixture() {
            mLevel.insertChunk(LevelChunk(0, 0));
            mState = VanillaBlocks::fromIdentifier("minecraft:dispenser")->toBlockState();
            Tag states = mState.mStates;
            states.putInt("facing_direction", FACING_EAST);
            mState = BlockState(mState.mName, states);
            mLevel.setBlock(AT, mState, false);
            mActor = &mLevel.getBlockActors().getOrCreate<DispenserBlockActor>(AT);
        }

        void load(const std::string &identifier, int32_t count, int32_t aux = 0) {
            ItemStack stack = mOwner.createItemStack(identifier, count);
            stack.mDamage = aux;
            mActor->getInventory().setContainerItem(0, stack);
        }

        void fire() {
            VanillaBlocks::fromIdentifier("minecraft:dispenser")->onScheduledUpdate(mOwner, mLevel, AT, mState);
        }

        int32_t count() const {
            const ItemStack &item = mActor->getInventory().getContainerItem(0);
            return item.isAir() ? 0 : item.mCount;
        }

        RecordingBlockActor &listen() {
            std::unique_ptr<RecordingBlockActor> probe(new RecordingBlockActor(Vector3i(AT.x, AT.y, AT.z + 3)));
            RecordingBlockActor &result = *probe;
            mLevel.getBlockActors().insert(std::move(probe));
            return result;
        }

        ServerActor *find(const std::string &identifier) {
            for (auto &entry: mOwner.getActors()) {
                if (identifier == entry.second->getIdentifier())
                    return entry.second.get();
            }
            return nullptr;
        }
    };

    // shot east from the dispense position with the definition's power, a stack loses one
    void projectiles() {
        Fixture fixture;
        fixture.load("minecraft:arrow", 2);
        fixture.fire();
        require(fixture.count() == 1, "the dispensed arrow was not taken from the stack");
        ServerActor *arrow = fixture.find("minecraft:arrow");
        require(arrow != nullptr, "the dispenser shot no arrow");
        require(std::fabs(arrow->getPosition().x - 9.2f) < 0.01f, "the arrow left from the wrong position");
        require(arrow->getMotion().x > 0.4f, "the arrow did not fly east");
        require(!arrow->getProjectileData().mPickupItem.isAir(), "a dispensed arrow cannot be picked up");

        Fixture potions;
        potions.load("minecraft:splash_potion", 1, 21);
        potions.fire();
        require(potions.count() == 0, "the splash potion stayed in the dispenser");
        PotionActor *potion = dynamic_cast<PotionActor *>(potions.find("minecraft:splash_potion"));
        require(potion != nullptr && potion->getPotionId() == 21, "the splash potion lost its potion");

        Fixture fireballs;
        fireballs.load("minecraft:fire_charge", 1);
        fireballs.fire();
        require(fireballs.count() == 0 && fireballs.find("minecraft:small_fireball") != nullptr,
                "the fire charge shot no small fireball");

        Fixture snowballs;
        snowballs.load("minecraft:snowball", 1);
        snowballs.fire();
        ServerActor *snowball = snowballs.find("minecraft:snowball");
        require(snowball != nullptr && snowball->getMotion().x > 1.4f, "the snowball did not fly at power 1.5");
    }

    // primed only while tntexplodes is on, else dropped
    void tnt() {
        Fixture fixture;
        RecordingBlockActor &probe = fixture.listen();
        fixture.load("minecraft:tnt", 2);
        fixture.fire();
        require(fixture.count() == 1 && fixture.find("minecraft:tnt") != nullptr, "the dispenser primed no TNT");
        require(probe.mEvents.size() == 1 && probe.mEvents[0] == &GameEvents::ENTITY_PLACE,
                "the primed TNT posted no entity_place");

        Fixture disabled;
        disabled.mLevel.getGameRules().setFromString("tntexplodes", "false");
        disabled.load("minecraft:tnt", 2);
        disabled.fire();
        require(disabled.count() == 1 && disabled.find("minecraft:tnt") == nullptr,
                "TNT was primed with tntexplodes off");
    }

    // a crop in front grows and takes one; nothing to grow keeps the bone meal
    void boneMeal() {
        Fixture fixture;
        const Vector3i crop(AT.x + 1, AT.y, AT.z);
        fixture.mLevel.setBlock(crop, VanillaBlocks::fromIdentifier("minecraft:wheat")->toBlockState(), false);
        fixture.load("minecraft:bone_meal", 2);
        fixture.fire();
        require(fixture.count() == 1, "the bone meal was not used on the crop");
        require(fixture.mLevel.getBlockState(crop.x, crop.y, crop.z).mStates.getInt("growth") > 0,
                "the crop did not grow");

        Fixture empty;
        empty.load("minecraft:bone_meal", 2);
        empty.fire();
        require(empty.count() == 2, "bone meal with nothing to grow left the dispenser");
    }

    // Item::dispense refuses: the item drops as from a dropper
    void dropped() {
        Fixture fixture;
        fixture.load("minecraft:ender_pearl", 3);
        fixture.fire();
        require(fixture.count() == 2, "the ender pearl was not dropped");
        require(fixture.find("minecraft:ender_pearl") == nullptr, "the dispenser threw an ender pearl");

        Fixture empty;
        RecordingBlockActor &probe = empty.listen();
        empty.fire();
        require(empty.mOwner.getActors().empty(), "an empty dispenser spawned something");
        require(probe.mEvents.size() == 1 && probe.mEvents[0] == &GameEvents::DISPENSE_FAIL,
                "an empty dispenser posted no dispense_fail");
        require(probe.mPositions[0].x == 8.5f && probe.mPositions[0].y == 100.5f,
                "dispense_fail was not posted at the dispenser center");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-dispenser-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        projectiles();
        tnt();
        boneMeal();
        dropped();
    } catch (const std::exception &error) {
        std::clog << "Dispenser regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
