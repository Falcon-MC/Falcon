#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorMovementSystem.h"
#include "World/Actor/Movement/BuoyancySystem.h"
#include "World/Actor/Movement/PushableByEntity.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Spawn/SpawnRules.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/Item.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/World.h"
#include "Protocol/Types/ItemDefinition.h"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const char *const ARCHETYPE = "minecraft:sulfur_cube_archetype";
    const char *const TAG_PREFIX = "minecraft:sulfur_cube_archetype_";
    const int32_t FLOOR_Y = 63;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack stackOf(const std::string &identifier) {
        ItemStack item;
        item.mDefinition = std::make_shared<ItemDefinition>(identifier, 1, false, Tag());
        item.mCount = 1;
        return item;
    }

    struct Fixture {
        ServerNetworkHandler mOwner{"Sulfur cube", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Fixture() {
            for (int32_t x = -2; x <= 1; x++) {
                for (int32_t z = -2; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            const BlockState stone = VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState();
            for (int32_t x = -32; x < 32; x++) {
                for (int32_t z = -32; z < 32; z++)
                    mLevel.setBlockState(x, FLOOR_Y, z, stone);
            }
        }

        MobActor &cube(const Vector3f &position) {
            ServerActor *actor = mOwner.spawnActor(mLevel, "minecraft:sulfur_cube", position, [](ServerActor &spawned) {
                static_cast<MobActor &>(spawned).setSpawnEvent("minecraft:spawn_medium");
            });
            require(actor != nullptr, "the sulfur cube spawns");
            MobActor &mob = *static_cast<MobActor *>(actor);
            mob.startDefinition(mOwner);
            // the first look at the hand only learns what it holds
            mob.tick(mOwner);
            return mob;
        }

        void feed(MobActor &mob, const std::string &block) {
            mob.getEquipment().setSlot(MobEquipment::MAINHAND, stackOf(block));
            mob.tick(mOwner);
        }

        void empty(MobActor &mob) {
            mob.getEquipment().setSlot(MobEquipment::MAINHAND, ItemStack::air());
            mob.tick(mOwner);
        }
    };

    std::string archetypeOf(const MobActor &mob) {
        const ActorPropertyDescription *description = mob.findPropertyDescription(ARCHETYPE);
        require(description != nullptr, "the archetype property exists");
        const int32_t index = mob.getIntProperty(ARCHETYPE, -1);
        require(index >= 0 && (size_t) index < description->mEnumValues.size(), "the archetype is a valid enum");
        return description->mEnumValues[(size_t) index];
    }

    // every archetype tag, every block in it: the block a cube absorbs picks its archetype
    void checkArchetypes() {
        Fixture fixture;
        MobActor &mob = fixture.cube(Vector3f(0.5f, 64.0f, 0.5f));
        require(archetypeOf(mob) == "none", "a medium cube starts without archetype");

        std::map<std::string, std::vector<std::string>> owners;
        size_t blocks = 0;
        std::vector<std::string> identifiers;
        for (const std::unique_ptr<Item> &item: VanillaItems::getAll())
            identifiers.emplace_back(item->getIdentifier());
        for (const std::unique_ptr<Block> &block: VanillaBlocks::getAll())
            identifiers.push_back(block->toBlockState().mName);
        for (const std::string &identifier: identifiers) {
            if (owners.count(identifier) != 0)
                continue;
            for (const std::string &tag: CraftingRecipeTable::getItemTags(identifier)) {
                if (tag.rfind(TAG_PREFIX, 0) == 0)
                    owners[identifier].push_back(tag.substr(std::string(TAG_PREFIX).size()));
            }
        }
        require(owners.size() >= 300, "the archetype tags list the absorbable blocks");

        for (const auto &entry: owners) {
            require(entry.second.size() == 1, entry.first + " belongs to one archetype only");
            fixture.feed(mob, entry.first);
            require(mob.hasComponentGroup("minecraft:sulfur_cube_medium_with_block"), entry.first + " is absorbed");
            require(mob.hasComponentGroup("minecraft:sulfur_cube_" + entry.second.front()),
                    entry.first + " adds its archetype group");
            require(archetypeOf(mob) == entry.second.front(), entry.first + " picks " + entry.second.front());

            fixture.empty(mob);
            require(archetypeOf(mob) == "none", entry.first + " ejected resets the archetype");
            require(mob.hasComponentGroup("minecraft:sulfur_cube_medium_without_block"),
                    entry.first + " ejected gives the cube its AI back");
            blocks++;
        }
        require(blocks == 337, "the 12 archetype tags hold 337 blocks");
    }

    // the IDA formulas, written out again: ComputeRestitution sub_1484C9200, ApplyGravityWithBounceCorrection
    // sub_143368B50, MobMovementFriction sub_1433563E0 / sub_1433307F0
    struct Expected {
        float mX;
        float mY;
        float mVelocityX;
        float mVelocityY;
        bool mOnGround;
    };

    float keep(float drag) {
        return drag <= 1.0f ? 1.0f - std::max(0.0f, drag) : 0.0f;
    }

    void step(Expected &state, float bounciness, float frictionModifier, float airDragModifier) {
        const float gravity = 0.08f;
        const float floor = (float) FLOOR_Y + 1.0f;
        const float velocityY = state.mVelocityY;
        float moved = velocityY;
        bool collided = false;
        if (state.mY + velocityY < floor) {
            moved = floor - state.mY;
            collided = true;
        }
        state.mOnGround = (collided && velocityY < 0.0f) || (state.mOnGround && !collided && velocityY == 0.0f);
        state.mY += moved;
        state.mX += state.mVelocityX;

        float vy = collided ? 0.0f : velocityY;
        float restitution = 0.0f;
        if (collided && (velocityY >= 0.0f || std::fabs(velocityY) >= 0.08000012f))
            restitution = -velocityY * bounciness;
        vy += restitution;

        float pull = gravity;
        if (std::fabs(restitution) > 0.00000011920929f && std::fabs(moved) > 0.00000011920929f) {
            const float sign = velocityY > 0.0f ? 1.0f : -1.0f;
            const float reached = std::fabs(
                    (std::fabs(velocityY) - std::sqrt(2.0f * gravity * std::fabs(moved) + velocityY * velocityY))
                    / gravity);
            pull = -((1.0f - reached) * sign * gravity);
        }
        vy -= pull;

        const float friction = state.mOnGround ? 0.6f : 1.0f;
        const float factor = keep((1.0f - friction) * frictionModifier) * keep(0.089999974f * airDragModifier);
        state.mVelocityX = std::fabs(state.mVelocityX) <= 0.00000011920929f ? 0.0f : state.mVelocityX * factor;
        state.mVelocityY = std::fabs(vy) <= 0.00000011920929f ? 0.0f : vy * factor;
    }

    void near(float actual, float expected, const std::string &message) {
        require(std::isfinite(actual) && std::fabs(actual - expected) < 0.0005f,
                message + ": " + std::to_string(actual) + " instead of " + std::to_string(expected));
    }

    struct PhysicsCase {
        const char *mBlock;
        const char *mArchetype;
        float mBounciness;
        float mFrictionModifier;
        float mAirDragModifier;
    };

    void checkPhysics() {
        // the values of the archetype groups in the definition
        const PhysicsCase cases[] = {
                {"minecraft:oak_log", "bouncy", 0.9f, 0.3f, 0.01f},
                {"minecraft:honeycomb_block", "sticky", 0.0f, 2.0f, 0.01f},
                {"minecraft:blue_ice", "fast_sliding", 0.1f, 0.05f, 0.01f},
                {"minecraft:white_wool", "light", 1.0f, 0.3f, 1.8f}
        };
        for (const PhysicsCase &test: cases) {
            Fixture fixture;
            MobActor &mob = fixture.cube(Vector3f(0.5f, 66.0f, 0.5f));
            fixture.feed(mob, test.mBlock);
            require(archetypeOf(mob) == test.mArchetype, std::string(test.mBlock) + " archetype for the physics case");
            const PhysicsComponent physics = mob.getPhysics();
            require(physics.mDefinitionFriction && physics.mUniformAirDrag, "the cube moves by its attributes");
            near(physics.mBounciness, test.mBounciness, "bounciness from the group");

            mob.setPosition(Vector3f(0.5f, 66.0f, 0.5f));
            mob.setMotion(Vector3f(0.3f, 0.0f, 0.0f));
            mob.setOnGround(false);
            Expected expected{0.5f, 66.0f, 0.3f, 0.0f, false};
            bool bounced = false;
            for (int32_t tick = 0; tick < 60; tick++) {
                ActorMovementSystem::tick(fixture.mOwner, mob);
                const float before = expected.mVelocityY;
                step(expected, test.mBounciness, test.mFrictionModifier, test.mAirDragModifier);
                bounced = bounced || (before < -0.08f && expected.mVelocityY > 0.0f);
                const std::string at = std::string(test.mArchetype) + " tick " + std::to_string(tick);
                near(mob.getPosition().y, expected.mY, at + " height");
                near(mob.getPosition().x, expected.mX, at + " distance");
                near(mob.getMotion().y, expected.mVelocityY, at + " vertical speed");
                near(mob.getMotion().x, expected.mVelocityX, at + " horizontal speed");
            }
            require(bounced == (test.mBounciness > 0.0f), std::string(test.mArchetype) + " bounces as its bounciness");
        }
    }

    // a hot cube's magma damage primes an explosive cube; its fuse then blows it up
    void checkHotPrimesExplosive() {
        Fixture fixture;
        MobActor &hot = fixture.cube(Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &explosive = fixture.cube(Vector3f(1.6f, 64.0f, 0.5f));
        MobActor &regular = fixture.cube(Vector3f(-0.6f, 64.0f, 0.5f));
        fixture.feed(explosive, "minecraft:tnt");
        fixture.feed(regular, "minecraft:dirt");
        const float regularHealth = regular.getHealth();
        // the hot cube attacks in the tick it absorbs its magma
        fixture.feed(hot, "minecraft:magma");
        require(archetypeOf(hot) == "hot" && archetypeOf(explosive) == "explosive" && archetypeOf(regular) == "regular",
                "the three archetypes");
        require(explosive.hasComponentGroup("minecraft:sulfur_cube_medium_primed"),
                "magma damage from the hot cube primes the explosive cube");
        require(!explosive.hasComponentGroup("minecraft:sulfur_cube_medium_with_block_interactable"),
                "priming removes the interactions");
        require(regular.getHealth() == regularHealth, "the hot cube spares other cubes");

        for (int32_t tick = 0; tick < 6 * 20 + 2 && !explosive.isExpired(); tick++)
            explosive.tick(fixture.mOwner);
        require(explosive.isExpired(), "the primed cube explodes after its fuse");
    }

    void checkFrostWalker() {
        Fixture fixture;
        MobActor &hot = fixture.cube(Vector3f(0.5f, 64.0f, 0.5f));
        fixture.feed(hot, "minecraft:magma");
        const json::Value *attack = hot.getComponent("minecraft:area_attack");
        require(attack != nullptr && attack->get("entity_filter") != nullptr, "the hot cube has an area attack");
        const json::Value &filter = *attack->get("entity_filter");

        NetworkIdentifier id;
        ServerPlayer &player = fixture.mOwner.getPlayers().try_emplace(id, id, 9001).first->second;
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        require(EntityFilter::test(filter, fixture.mOwner, hot, &player), "a player without frost walker burns");

        const EnchantmentData *frostWalker = EnchantmentTable::findByName("frost_walker");
        require(frostWalker != nullptr, "frost walker is known");
        ItemStack boots = stackOf("minecraft:diamond_boots");
        ItemEnchantments::write(boots, {EnchantmentInstance{frostWalker->mId, 1}});
        player.getInventory().setArmor(PlayerInventory::ARMOR_FEET, boots);
        require(!EntityFilter::test(filter, fixture.mOwner, hot, &player), "frost walker boots walk over the hot cube");
    }

    // pushable_by_entity ball: sub_143A0C040 with the regular preset
    void checkKick() {
        const auto preset = json::parse(R"({"push_mode":"ball","min_distance":0.0,"max_distance":1.0,
            "require_collision_overlap":false,"push_scale_self":2.0,"kick_speed_scale":0.3,"min_kick_speed":0.0,
            "max_kick_speed":0.5,"vertical_kick_multiplier":0.3,"play_sound":true,"play_sound_impulse_threshold":0.1,
            "play_sound_cooldown_in_seconds":0.7})");
        const PushableByEntityPreset settings = PushableByEntityPreset::read(*preset);
        require(settings.mBall && settings.mSoundCooldownTicks == 14, "the ball preset parses");

        const Vector3f grounded = PushableByEntity::kick(settings, 1.0f, 0.0f, 0.5f, true);
        near(grounded.x, -2.0f * 0.15f, "kicked away from the pusher");
        near(grounded.y, 2.0f * 0.3f * 0.15f, "a grounded cube is lifted");
        near(grounded.z, 0.0f, "no sideways kick");
        const Vector3f airborne = PushableByEntity::kick(settings, 0.0f, -2.0f, 10.0f, false);
        near(airborne.z, 2.0f * 0.5f, "the kick speed is capped");
        near(airborne.y, 0.0f, "an airborne cube is not lifted");
        require(PushableByEntity::inReach(settings, 1.2f, 0.0f, 0.6f), "reach counts half the pusher width");
        require(!PushableByEntity::inReach(settings, 1.4f, 0.0f, 0.6f), "too far for the preset");
    }

    // BuoyancyFloat sub_146959470 with the bobbing timer of sub_14695ACE0
    void checkBuoyancy() {
        const auto component = json::parse(R"({"base_buoyancy":1.0,"apply_gravity":false,"movement_type":"bobbing",
            "liquid_blocks":["minecraft:water","minecraft:flowing_water","minecraft:lava","minecraft:flowing_lava"]})");
        const BuoyancySettings settings = BuoyancySystem::read(component.get());
        require(settings.mEnabled && settings.mWater && settings.mLava && !settings.mApplyGravity
                && settings.mMovement == BuoyancyMovement::Bobbing, "the buoyant component parses");

        std::mt19937 random(1);
        near((float) BuoyancySystem::advanceTimer(4.0, settings, Vector3f(1.0f, 0.0f, 0.0f), random), 5.0f,
             "bobbing advances one per tick");
        const float buoyancy = std::min(1.0f, (1.0f - 0.25f) * 0.9f + 0.1f);
        const float wave = ((float) std::sin(5.0 * 0.3141592741012573) + 1.2f) * 0.5f;
        near(BuoyancySystem::floatVelocity(-0.2f, 62.25f, 5.0, settings),
             std::min((buoyancy - wave - 0.1f) * 0.15f, -0.2f * 0.7f + 0.05f), "the float lift");
    }

    void checkSpawnRule() {
        const SpawnRule *rule = nullptr;
        for (const SpawnRule &candidate: SpawnRules::getRules()) {
            if (candidate.mIdentifier == "minecraft:sulfur_cube")
                rule = &candidate;
        }
        require(rule != nullptr && rule->mPopulation == "monster", "the sulfur cube has a monster spawn rule");
        require(rule->mConditions.size() == 1, "one spawn condition");
        const SpawnCondition &condition = rule->mConditions.front();
        require(condition.mSupported && condition.mOnSurface && condition.mUnderground, "surface and underground");
        require(condition.mWeight == 150, "weight 150");
        require(condition.mHerds.size() == 1 && condition.mHerds.front().mMinSize == 2
                && condition.mHerds.front().mMaxSize == 4, "herds of 2 to 4");
        require(condition.mBiomeFilter != nullptr, "a biome filter");
        const json::Value &filter = *condition.mBiomeFilter;
        const json::Value &test = filter.isArray() ? *filter.mArray.front() : filter;
        require(test.get("value") != nullptr && test.get("value")->string() == "sulfur_caves", "only sulfur caves");

        BiomeChunkGenDataRegistry::initialize();
        const int32_t caves = BiomeChunkGenDataRegistry::getBiomeId("sulfur_caves");
        require(caves >= 0 && BiomeChunkGenDataRegistry::hasTag(caves, "sulfur_caves"),
                "the sulfur caves biome carries its tag");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        SpawnRules::initialize();
        checkKick();
        checkBuoyancy();
        checkSpawnRule();
        checkArchetypes();
        checkPhysics();
        checkHotPrimesExplosive();
        checkFrostWalker();
    } catch (const std::exception &exception) {
        std::cout << "Sulfur cube check failed: " << exception.what() << '\n';
        return 1;
    }

    std::cout << "Sulfur cube checks passed" << std::endl;
    return 0;
}
