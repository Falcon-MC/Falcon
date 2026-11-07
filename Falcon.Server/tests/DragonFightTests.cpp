#include "World/Actor/Mob/Hostile/EnderDragon.h"
#include "World/Level/Dimension/EndDragonFight.h"
#include "World/Level/LevelGen/End/Feature/EndSpikes.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    using Stage = EndDragonFight::RespawnStage;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void checkRespawnTimeline() {
        EndDragonFight::RespawnStep step = EndDragonFight::stepRespawn(Stage::Start, 0, EndSpikes::COUNT);
        require(step.mBeamToSummonPoint && step.mNextStage == Stage::PreparingToSummonPillars && step.mRestartTimer,
                "The ritual start must aim the crystals at the summon point and prepare the pillars");

        std::vector<int32_t> sounds;
        for (int32_t tick = 0; tick < 100; tick++) {
            step = EndDragonFight::stepRespawn(Stage::PreparingToSummonPillars, tick, EndSpikes::COUNT);
            require(step.mNextStage == Stage::PreparingToSummonPillars, "The pillars are prepared for 100 ticks");
            if (step.mPlaySummonSound)
                sounds.push_back(tick);
        }
        const std::vector<int32_t> expectedSounds = {0, 50, 51, 52, 95, 96, 97, 98, 99};
        require(sounds == expectedSounds, "The pillar preparation sounds on ticks 0, 50 to 52 and from 95");
        step = EndDragonFight::stepRespawn(Stage::PreparingToSummonPillars, 100, EndSpikes::COUNT);
        require(step.mNextStage == Stage::SummoningPillars && step.mRestartTimer,
                "The pillars are summoned after 100 ticks of preparation");

        for (int32_t spike = 0; spike < EndSpikes::COUNT; spike++) {
            step = EndDragonFight::stepRespawn(Stage::SummoningPillars, spike * 40, EndSpikes::COUNT);
            require(step.mBeamToSpike == spike, "Each spike is aimed at on the first tick of its 40");
            step = EndDragonFight::stepRespawn(Stage::SummoningPillars, spike * 40 + 38, EndSpikes::COUNT);
            require(step.mClearSpike == spike, "Each spike area is cleared on tick 38");
            step = EndDragonFight::stepRespawn(Stage::SummoningPillars, spike * 40 + 39, EndSpikes::COUNT);
            require(step.mRebuildSpike == spike, "Each spike is rebuilt on tick 39");
            step = EndDragonFight::stepRespawn(Stage::SummoningPillars, spike * 40 + 20, EndSpikes::COUNT);
            require(step.mBeamToSpike < 0 && step.mClearSpike < 0 && step.mRebuildSpike < 0,
                    "Nothing happens between the spike ticks");
        }
        step = EndDragonFight::stepRespawn(Stage::SummoningPillars, 400, EndSpikes::COUNT);
        require(step.mNextStage == Stage::SummoningDragon && step.mRestartTimer,
                "The dragon is summoned once the ten spikes stand");

        step = EndDragonFight::stepRespawn(Stage::SummoningDragon, 0, EndSpikes::COUNT);
        require(step.mBeamToSummonPoint && !step.mPlaySummonSound, "The summon starts by aiming at the summon point");
        for (int32_t tick = 1; tick < 100; tick++) {
            step = EndDragonFight::stepRespawn(Stage::SummoningDragon, tick, EndSpikes::COUNT);
            const bool expected = tick <= 4 || tick >= 80;
            require(step.mPlaySummonSound == expected, "The summon sounds on ticks 1 to 4 and from 80");
            require(!step.mSummonDragon, "The dragon only appears after 100 ticks");
        }
        step = EndDragonFight::stepRespawn(Stage::SummoningDragon, 100, EndSpikes::COUNT);
        require(step.mSummonDragon && step.mNextStage == Stage::None, "The ritual ends with the new dragon");
    }

    void checkGateways() {
        const std::vector<int32_t> order = EndDragonFight::createGatewayOrder(42);
        require(order == EndDragonFight::createGatewayOrder(42), "The gateway order depends only on the seed");
        require(order != EndDragonFight::createGatewayOrder(43), "Another seed opens the gateways in another order");

        std::vector<int32_t> sorted = order;
        std::sort(sorted.begin(), sorted.end());
        for (int32_t i = 0; i < EndDragonFight::GATEWAY_COUNT; i++)
            require(sorted[(size_t) i] == i, "The gateway order holds each of the 20 gateways once");

        const Vector3i first = EndDragonFight::getGatewayPosition(0);
        require(first.x == 96 && first.y == 75 && first.z == 0, "Gateway 0 opens east of the island");
        const Vector3i quarter = EndDragonFight::getGatewayPosition(5);
        require(quarter.x == 0 && quarter.y == 75 && quarter.z == 96, "Gateway 5 opens south of the island");
        const Vector3i half = EndDragonFight::getGatewayPosition(10);
        require(half.x == -96 && half.z == 0, "Gateway 10 opens west of the island");

        std::set<std::pair<int32_t, int32_t>> positions;
        for (int32_t i = 0; i < EndDragonFight::GATEWAY_COUNT; i++) {
            const Vector3i position = EndDragonFight::getGatewayPosition(i);
            const int32_t radiusSquared = position.x * position.x + position.z * position.z;
            require(radiusSquared >= 94 * 94 && radiusSquared <= 96 * 96, "Gateways ring the island 96 blocks out");
            positions.insert({position.x, position.z});
        }
        require(positions.size() == (size_t) EndDragonFight::GATEWAY_COUNT, "Each gateway has its own place");
    }

    void checkExperience() {
        int32_t first = 0;
        int32_t again = 0;
        for (int32_t tick = 1; tick <= EndDragonFight::DEATH_DURATION; tick++) {
            first += EndDragonFight::getDeathExperience(tick, false);
            again += EndDragonFight::getDeathExperience(tick, true);
        }

        require(first == EndDragonFight::FIRST_KILL_EXPERIENCE, "The first dragon drops 12000 experience");
        require(again == EndDragonFight::REPEAT_KILL_EXPERIENCE, "A respawned dragon drops 500 experience");
        require(EndDragonFight::getDeathExperience(150, false) == 0, "No experience falls before tick 151");
        require(EndDragonFight::getDeathExperience(155, false) == 960, "Each burst drops 8% of the reward");
    }

    void checkPersistence() {
        Tag data = Tag::ofCompound();
        {
            EndDragonFight fresh(7);
            fresh.save(data);
        }

        const Tag *saved = data.get("DragonFight");
        require(saved != nullptr && saved->isCompound(), "The fight saves under DragonFight");
        require(saved->getLong("DragonUUID") == -1, "A fight without a dragon saves its id as -1");
        require(!saved->contains("ExitPortalLocation"), "No exit portal location is saved before one is known");
        const Tag *gateways = saved->get("Gateways");
        require(gateways != nullptr && gateways->size() == (size_t) EndDragonFight::GATEWAY_COUNT,
                "A new fight saves its 20 gateways");

        Tag fight = Tag::ofCompound();
        fight.putLong("DragonUUID", 1234);
        fight.putBool("DragonKilled", true);
        fight.putBool("DragonSpawned", true);
        fight.putBool("PreviouslyKilled", true);
        fight.putBool("IsRespawning", true);
        fight.putByte("DragonFightVersion", 1);
        Tag exit = Tag::ofList(Tag::Type::Int);
        exit.addToList(Tag::ofInt(0));
        exit.addToList(Tag::ofInt(64));
        exit.addToList(Tag::ofInt(0));
        fight.put("ExitPortalLocation", exit);
        Tag order = Tag::ofList(Tag::Type::Int);
        order.addToList(Tag::ofInt(3));
        order.addToList(Tag::ofInt(17));
        fight.put("Gateways", order);
        Tag stored = Tag::ofCompound();
        stored.put("DragonFight", fight);

        EndDragonFight loaded(7);
        loaded.load(stored);
        require(loaded.isDragonKilled() && loaded.wasPreviouslyKilled() && loaded.isDragonSpawned(),
                "The kill state is read back");
        require(loaded.getDragonUniqueId() == 1234, "The dragon id is read back");
        require(loaded.getRespawnStage() == Stage::Start, "A fight saved while respawning restarts the ritual");
        require(loaded.getExitPortalLocation().has_value() && loaded.getExitPortalLocation()->y == 64,
                "The exit portal location is read back");
        require(loaded.getGateways() == std::vector<int32_t>({3, 17}), "The remaining gateways are read back");

        Tag resaved = Tag::ofCompound();
        loaded.save(resaved);
        require(resaved.get("DragonFight")->getByte("IsRespawning") == 1, "The ritual is saved as IsRespawning");
        require(resaved.get("DragonFight")->getByte("DragonFightVersion") == 1, "The fight saves its version");

        fight.remove("DragonFightVersion");
        Tag legacy = Tag::ofCompound();
        legacy.put("DragonFight", fight);
        EndDragonFight old(7);
        old.load(legacy);
        require(old.isDragonKilled() && !old.wasPreviouslyKilled(),
                "A fight saved before the version tag forgets the earlier kill");

        fight.remove("Gateways");
        Tag withoutGateways = Tag::ofCompound();
        withoutGateways.put("DragonFight", fight);
        EndDragonFight regenerated(7);
        regenerated.load(withoutGateways);
        require(regenerated.getGateways() == EndDragonFight::createGatewayOrder(7),
                "A fight without gateways shuffles them from the seed");
    }

    void checkSpikes() {
        const std::vector<EndSpike> spikes = EndSpikes::create(42);
        require(spikes.size() == (size_t) EndSpikes::COUNT, "The End has ten spikes");

        std::set<int32_t> heights;
        int32_t guarded = 0;
        for (const EndSpike &spike: spikes) {
            heights.insert(spike.mHeight);
            guarded += spike.mGuarded ? 1 : 0;
            require((spike.mHeight - 76) % 3 == 0 && spike.mRadius == (spike.mHeight - 76) / 9 + 2,
                    "A spike's radius follows its height");
            const Vector3f crystal = spike.getCrystalPosition();
            require(crystal.y == (float) spike.mHeight + 1.0f, "The crystal stands on the spike's bedrock");
        }
        require(heights.size() == (size_t) EndSpikes::COUNT, "Each spike has its own height");
        require(guarded == 2, "Two spikes keep their crystal in a cage");

        int32_t cleared = 0;
        int32_t obsidian = 0;
        EndSpikes::place(spikes[0], true, [&](int32_t, int32_t y, int32_t, const BlockState &state) {
            if (state.mName == "minecraft:air") {
                cleared++;
                require(y >= EndSpikes::CLEARED_FROM_Y, "Only the column above y 66 is cleared");
            } else if (state.mName == "minecraft:obsidian") {
                obsidian++;
            }
        });
        require(cleared > 0 && obsidian > 0, "A rebuilt spike clears its column and stacks obsidian");

        int32_t untouched = 0;
        EndSpikes::place(spikes[0], false, [&](int32_t, int32_t, int32_t, const BlockState &state) {
            untouched += state.mName == "minecraft:air" ? 1 : 0;
        });
        require(untouched == 0, "The first dragon spawn does not clear around a spike it puts back");
    }

    void checkNodes() {
        EnderDragon dragon(1, EnderDragon::IDENTIFIER);
        dragon.buildNodes([](int32_t, int32_t) {
            return 60;
        }, 32);

        const std::vector<Vector3i> &nodes = dragon.getNodes();
        require(nodes.size() == (size_t) EnderDragon::NODE_COUNT, "The dragon flies over 24 nodes");
        require(nodes[0].x == 60 && nodes[0].y == 65 && nodes[0].z == 0, "The first outer node is 60 blocks east");
        require(nodes[12].y == 75 && nodes[20].y == 65, "Middle nodes fly 15 blocks up and inner ones 5");

        dragon.setCrystalsAlive(10);
        const std::unique_ptr<EnderDragon::Path> path = dragon.findPath(0, 6, std::nullopt);
        require(path != nullptr && path->getNodes().front() == nodes[0] && path->getNodes().back() == nodes[6],
                "A path joins its two nodes");
        for (size_t i = 1; i < path->getNodes().size(); i++) {
            const auto from = std::find(nodes.begin(), nodes.end(), path->getNodes()[i - 1]) - nodes.begin();
            const auto to = std::find(nodes.begin(), nodes.end(), path->getNodes()[i]) - nodes.begin();
            require((EnderDragon::getNodeAdjacency((int32_t) from) & (1u << to)) != 0,
                    "A path only follows linked nodes");
        }

        const std::unique_ptr<EnderDragon::Path> landing = dragon.findPath(3, 20, Vector3i(0, 70, 0));
        require(landing != nullptr && landing->getNodes().back() == Vector3i(0, 70, 0),
                "A landing path ends over the portal");

        dragon.setCrystalsAlive(0);
        const std::unique_ptr<EnderDragon::Path> inner = dragon.findPath(12, 19, std::nullopt);
        require(inner != nullptr, "Without crystals the dragon keeps to the inner nodes");
        for (const Vector3i &node: inner->getNodes()) {
            const auto index = std::find(nodes.begin(), nodes.end(), node) - nodes.begin();
            require(index >= 12, "Without crystals no path goes back to the outer ring");
        }
    }
}

int main() {
    try {
        checkRespawnTimeline();
        checkGateways();
        checkExperience();
        checkPersistence();
        checkSpikes();
        checkNodes();
    } catch (const std::exception &exception) {
        std::cerr << "Dragon fight check failed: " << exception.what() << std::endl;
        return 1;
    }

    std::cout << "Dragon fight checks passed" << std::endl;
    return 0;
}
