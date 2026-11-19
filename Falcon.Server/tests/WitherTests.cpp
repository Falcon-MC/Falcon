#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/WitherActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/SkullBlockActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const int32_t FACING_SOUTH = 3;
    const int32_t FACING_EAST = 5;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Wither", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -3; x <= 3; x++) {
                for (int32_t z = -3; z <= 3; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }

        size_t countOf(const char *identifier) {
            size_t found = 0;
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    ++found;
            }
            return found;
        }

        WitherActor *findWither() {
            for (auto &entry: mOwner.getActors()) {
                WitherActor *wither = dynamic_cast<WitherActor *>(entry.second.get());
                if (wither != nullptr && !wither->isDead())
                    return wither;
            }
            return nullptr;
        }

        std::string nameAt(const Vector3i &position) {
            return mLevel->getBlockState(position.x, position.y, position.z).mName;
        }
    };

    BlockState skullState() {
        return VanillaBlocks::fromIdentifier(BlockNames::WITHER_SKELETON_SKULL)->toBlockState();
    }

    // the soul sand T along x, centred on top on (x, y + 2, z), with the first `skulls` skulls from -x
    void buildT(Arena &arena, const Vector3i &top, int32_t skulls) {
        Level &level = *arena.mLevel;
        const BlockState soul(BlockNames::SOUL_SAND);
        for (int32_t dx = -1; dx <= 1; ++dx)
            level.setBlockState(top.x + dx, top.y - 1, top.z, soul);
        level.setBlockState(top.x, top.y - 2, top.z, soul);
        for (int32_t dx = -1; dx < -1 + skulls; ++dx)
            level.setBlock(Vector3i(top.x + dx, top.y, top.z), skullState(), false);
    }

    ActorDamageSource generic() {
        return ActorDamageSource::environment("death.attack.generic", "Wither").withoutArmor().withoutCooldown();
    }

    void summon(Arena &arena) {
        const Vector3i top(-20, 102, -20);
        buildT(arena, top, 2);
        require(!WitherActor::trySummon(arena.mOwner, *arena.mLevel, Vector3i(top.x, top.y, top.z)),
                "two skulls summoned a wither");
        require(arena.countOf(WitherActor::IDENTIFIER) == 0, "a wither appeared without its third skull");

        arena.mLevel->setBlock(Vector3i(top.x + 1, top.y, top.z), skullState(), false);
        require(WitherActor::trySummon(arena.mOwner, *arena.mLevel, Vector3i(top.x + 1, top.y, top.z)),
                "the full T did not summon a wither");
        require(arena.countOf(WitherActor::IDENTIFIER) == 1, "the T did not leave one wither");
        for (int32_t dx = -1; dx <= 1; ++dx) {
            require(arena.nameAt(Vector3i(top.x + dx, top.y, top.z)) == BlockNames::AIR, "a skull stayed");
            require(arena.nameAt(Vector3i(top.x + dx, top.y - 1, top.z)) == BlockNames::AIR, "an arm stayed");
        }
        require(arena.nameAt(Vector3i(top.x, top.y - 2, top.z)) == BlockNames::AIR, "the foot stayed");

        // origin + 1.5 in world axes: the origin is a corner of the T, either end of its arms
        const Vector3f position = arena.findWither()->getPosition();
        require(std::fabs(position.x - (float) top.x) <= 3.0f && std::fabs(position.z - (float) top.z) <= 2.0f
                        && std::fabs(position.y - (float) (top.y - 2)) <= 1.0f,
                "the wither did not appear at its T");

        // a block in a corner under the arms spoils the T
        const Vector3i spoiled(-30, 102, -20);
        buildT(arena, spoiled, 3);
        arena.mLevel->setBlockState(spoiled.x - 1, spoiled.y - 2, spoiled.z, BlockState("minecraft:stone"));
        require(!WitherActor::trySummon(arena.mOwner, *arena.mLevel, spoiled),
                "a T with a filled corner summoned a wither");
    }

    void spawnFrames(Arena &arena, WitherActor &wither) {
        ServerNetworkHandler &owner = arena.mOwner;
        owner.tickActors();
        const float max = wither.getMaxHealth();
        require(max == WitherActor::NORMAL_HEALTH, "the wither's health is not the normal difficulty's");
        require(wither.getHealth() > 0.0f && wither.getHealth() <= 1.0f + std::ceil(max / 200.0f),
                "the wither did not start its frames at 1 health");
        require(!wither.hurt(owner, 5.0f, generic()), "the wither was hurt during its spawn frames");

        for (int32_t tick = 1; tick < WitherActor::SPAWNING_FRAMES; ++tick)
            owner.tickActors();
        require(wither.getHealth() == max, "the spawn frames did not heal the wither to full");
        require(!wither.isDead(), "the wither died to its own spawn explosion");
        require(wither.hurt(owner, 5.0f, generic()), "the wither was still invulnerable after 200 frames");
        require(wither.getHealth() < max, "the hurt after the frames dealt nothing");
    }

    void halfHealth(Arena &arena, WitherActor &wither) {
        ServerNetworkHandler &owner = arena.mOwner;
        require(wither.canBeAffected(MobEffectId::InstantHealth), "instant health missed the wither");
        require(!wither.canBeAffected(MobEffectId::Poison), "poison took on the wither");

        require(wither.hurt(owner, wither.getHealth() - WitherActor::NORMAL_HEALTH / 2.0f + 10.0f, generic()),
                "the blow to half health was refused");
        require(!wither.canBeAffected(MobEffectId::InstantHealth), "an effect took below half health");

        const size_t skeletons = arena.countOf("minecraft:wither_skeleton");
        owner.tickActors();
        require(arena.countOf("minecraft:wither_skeleton") == skeletons + 3,
                "half health did not bring 3 wither skeletons on normal");

        // a weaker hit inside the hurt cooldown is refused anyway: wait it out first
        for (int32_t tick = 0; tick < 40 && wither.getNoDamageTicks() > 0; ++tick)
            owner.tickActors();
        require(wither.getNoDamageTicks() == 0, "the hurt cooldown never ended");
        require(!wither.hurt(owner, 5.0f, generic().asProjectile()), "a projectile hurt the armored wither");

        const float before = wither.getHealth();
        require(wither.hurt(owner, 5.0f, generic()), "a melee blow was refused below half health");
        require(wither.getHealth() < before, "a melee blow dealt nothing below half health");
    }

    void saveAndLoad(Arena &arena, WitherActor &wither) {
        const Tag data = wither.saveNbt();
        require(data.getInt("Phase", -1) == 0, "the phase below half health was not saved");
        require(data.getByte("AirAttack", 1) == 0, "the armor was not saved");
        require(data.getInt("SpawningFrames", -1) == 0, "the spawn frames were not saved");
        require(data.getInt("maxHealth", 0) == (int32_t) WitherActor::NORMAL_HEALTH, "maxHealth was not saved");

        ServerActor *copy = arena.mOwner.spawnActor(*arena.mLevel, WitherActor::IDENTIFIER, Vector3f(40.5f, 110.0f, 40.5f));
        WitherActor *loaded = dynamic_cast<WitherActor *>(copy);
        require(loaded != nullptr, "no wither to load into");
        loaded->loadNbt(data);
        const Tag again = loaded->saveNbt();
        for (const char *key: {"Invul", "ShieldHealth", "Phase", "SpawningFrames", "maxHealth", "lastHealthInterval",
                               "firerate"})
            require(again.getInt(key, -7) == data.getInt(key, -9), std::string("the reload changed ") + key);
        require(again.getByte("AirAttack", 7) == data.getByte("AirAttack", 9), "the reload changed AirAttack");

        // a reloaded wither past its frames does not start them again
        arena.mOwner.tickActors();
        require(!loaded->hurt(arena.mOwner, 5.0f, generic().asProjectile()),
                "the reloaded wither below half health took a projectile");
        require(loaded->hurt(arena.mOwner, 5.0f, generic()), "the reloaded wither restarted its spawn frames");
        loaded->kill(arena.mOwner);
    }

    void death(Arena &arena, WitherActor &wither) {
        const int64_t id = wither.getUniqueId();
        wither.kill(arena.mOwner);
        require(wither.isDead(), "the killed wither is not dying");
        for (int32_t tick = 0; tick < WitherActor::DEATH_FRAMES / 2; ++tick)
            arena.mOwner.tickActors();
        require(arena.mOwner.getActors().count(id) == 1, "the wither left before its death frames ended");
        for (int32_t tick = 0; tick < WitherActor::DEATH_FRAMES / 2 + 10; ++tick)
            arena.mOwner.tickActors();
        require(arena.mOwner.getActors().count(id) == 0, "the wither stayed after its death frames");
    }

    void skullRotation(Arena &arena) {
        require(SkullBlockActor::floorRotation(0.0f) == 0.0f, "yaw 0 did not face 0");
        require(SkullBlockActor::floorRotation(11.0f) == 0.0f, "yaw 11 left the first step");
        require(SkullBlockActor::floorRotation(12.0f) == 22.5f, "yaw 12 did not round to 22.5");
        require(SkullBlockActor::floorRotation(180.0f) == 180.0f, "yaw 180 did not face 180");
        require(SkullBlockActor::floorRotation(-90.0f) == -90.0f, "yaw -90 did not face -90");

        Level &level = *arena.mLevel;
        ServerPlayer &player = arena.player(9501, Vector3f(10.5f, 101.0f, 10.5f));
        Vector3f rotation = player.getRotation();
        rotation.y = 93.0f;
        player.setRotation(rotation);

        const Block *skeletonSkull = VanillaBlocks::fromIdentifier("minecraft:skeleton_skull");
        require(skeletonSkull != nullptr, "no skeleton skull block");
        const BlockState state = skeletonSkull->toBlockState();
        const ItemStack item = arena.mOwner.createItemStack("minecraft:skeleton_skull", 1);

        const Vector3i floor(12, 100, 10);
        level.setBlock(floor, state, false);
        skeletonSkull->onPlaced(arena.mOwner, player, floor, state, item, PlacementOrientation::FACE_UP);
        SkullBlockActor *placed = level.getBlockActors().find<SkullBlockActor>(floor);
        require(placed != nullptr, "a placed skull has no block actor");
        require(placed->saveNbt().getFloat("Rotation", -1.0f) == 90.0f, "a floor skull did not face the player");

        const Vector3i wall(14, 100, 10);
        level.setBlock(wall, state, false);
        skeletonSkull->onPlaced(arena.mOwner, player, wall, state, item, FACING_EAST);
        SkullBlockActor *hung = level.getBlockActors().find<SkullBlockActor>(wall);
        require(hung != nullptr && hung->saveNbt().getFloat("Rotation", -1.0f) == 0.0f,
                "a wall skull took a rotation");

        const Tag saved = placed->saveNbt();
        require((uint8_t) saved.getByte("SkullType", 0) == 0xFF, "an unset skull type is not 255");
        require(saved.getByte("DoingAnimation", 1) == 0 && saved.getInt("MouthTickCount", 1) == 0,
                "a still skull saved a mouth animation");

        Tag legacy = Tag::ofCompound();
        legacy.putByte("Rot", 4);
        legacy.putByte("MouthMoving", 1);
        SkullBlockActor old;
        old.loadNbt(legacy, arena.mOwner.getCodecContext());
        const Tag upgraded = old.saveNbt();
        require(upgraded.getFloat("Rotation", -1.0f) == 90.0f, "the old Rot steps were not read");
        require(upgraded.getByte("DoingAnimation", 0) == 1, "the old MouthMoving flag was not read");
    }

    struct Dispenser {
        Arena &mArena;
        Vector3i mAt;
        BlockState mState;
        DispenserBlockActor *mActor = nullptr;

        Dispenser(Arena &arena, const Vector3i &at, int32_t facing) : mArena(arena), mAt(at) {
            mState = VanillaBlocks::fromIdentifier("minecraft:dispenser")->toBlockState();
            Tag states = mState.mStates;
            states.putInt("facing_direction", facing);
            mState = BlockState(mState.mName, states);
            arena.mLevel->setBlock(at, mState, false);
            mActor = &arena.mLevel->getBlockActors().getOrCreate<DispenserBlockActor>(at);
            mActor->getInventory().setContainerItem(
                    0, arena.mOwner.createItemStack(BlockNames::WITHER_SKELETON_SKULL, 1));
        }

        void fire() {
            VanillaBlocks::fromIdentifier("minecraft:dispenser")
                    ->onScheduledUpdate(mArena.mOwner, *mArena.mLevel, mAt, mState);
        }

        bool empty() const {
            return mActor->getInventory().getContainerItem(0).isAir();
        }
    };

    void dispenser(Arena &arena) {
        // nothing near: the skull drops instead of landing in the air
        Dispenser lone(arena, Vector3i(30, 100, -30), FACING_EAST);
        lone.fire();
        require(lone.empty(), "the lone dispenser kept its skull");
        require(arena.nameAt(Vector3i(31, 100, -30)) == BlockNames::AIR, "a skull was placed with no T near");

        // two skulls short of the T: the one set in the middle gap stands at 180 degrees
        const Vector3i top(32, 102, -40);
        buildT(arena, top, 1);
        const size_t withers = arena.countOf(WitherActor::IDENTIFIER);
        Dispenser middle(arena, Vector3i(top.x, top.y, top.z - 1), FACING_SOUTH);
        middle.fire();
        require(middle.empty(), "the dispenser by the T kept its skull");
        require(arena.nameAt(top) == BlockNames::WITHER_SKELETON_SKULL, "the dispenser did not place the skull");
        SkullBlockActor *skull = arena.mLevel->getBlockActors().find<SkullBlockActor>(top);
        require(skull != nullptr && skull->saveNbt().getFloat("Rotation", -1.0f) == 180.0f,
                "the dispensed skull does not stand at 180 degrees");
        require(arena.countOf(WitherActor::IDENTIFIER) == withers, "two skulls summoned a wither");

        // the last skull, from a dispenser too, completes the T
        const Vector3i last(top.x + 1, top.y, top.z);
        Dispenser finishing(arena, Vector3i(last.x, last.y, last.z - 1), FACING_SOUTH);
        finishing.fire();
        require(arena.countOf(WitherActor::IDENTIFIER) == withers + 1, "the dispensed last skull summoned nothing");
        require(arena.nameAt(last) == BlockNames::AIR && arena.nameAt(top) == BlockNames::AIR,
                "the summon left skulls on the T");
    }

    void skullExplodesAfterHit(Arena &arena) {
        Level &level = *arena.mLevel;
        const BlockState dirt("minecraft:dirt");
        for (int32_t x = -1; x <= 1; ++x) {
            for (int32_t y = 119; y <= 121; ++y) {
                for (int32_t z = 34; z <= 35; ++z)
                    level.setBlockState(x, y, z, dirt);
            }
        }
        auto dirtLeft = [&]() {
            int32_t left = 0;
            for (int32_t x = -1; x <= 1; ++x) {
                for (int32_t y = 119; y <= 121; ++y) {
                    for (int32_t z = 34; z <= 35; ++z)
                        left += level.getBlockState(x, y, z).mName == "minecraft:dirt" ? 1 : 0;
                }
            }
            return left;
        };

        ServerActor *actor = arena.mOwner.spawnActor(level, "minecraft:wither_skull", Vector3f(0.5f, 120.5f, 30.5f));
        WitherSkullActor *skull = dynamic_cast<WitherSkullActor *>(actor);
        require(skull != nullptr, "the wither skull has no class");
        skull->setProjectile(true);
        const int64_t id = skull->getUniqueId();

        // a thrown projectile only hits from its third tick
        for (int32_t tick = 0; tick < 2; ++tick) {
            skull->setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            skull->setPosition(Vector3f(0.5f, 120.5f, 30.5f));
            arena.mOwner.tickActors();
        }
        skull->setMotion(Vector3f(0.0f, 0.0f, 1.0f));
        for (int32_t tick = 0; tick < 10 && !skull->isHeldAfterHit(); ++tick)
            arena.mOwner.tickActors();

        require(skull->isHeldAfterHit(), "the skull flew through the dirt");
        require(arena.mOwner.getActors().count(id) == 1, "the skull left on its hit");
        require(dirtLeft() == 18, "the skull exploded on the tick of its hit");
        const Vector3f motion = skull->getMotion();
        require(motion.x == 0.0f && motion.y == 0.0f && motion.z == 0.0f, "the held skull kept moving");

        arena.mOwner.tickActors();
        require(arena.mOwner.getActors().count(id) == 0, "the skull stayed past the tick after its hit");
        require(dirtLeft() < 18, "the skull's explosion broke nothing the tick after its hit");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-wither-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        summon(arena);
        WitherActor *wither = arena.findWither();
        require(wither != nullptr, "no wither after the summon");
        spawnFrames(arena, *wither);
        halfHealth(arena, *wither);
        saveAndLoad(arena, *wither);
        death(arena, *wither);
        skullRotation(arena);
        dispenser(arena);
        skullExplodesAfterHit(arena);
        std::cout << "wither checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
