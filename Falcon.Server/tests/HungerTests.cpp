#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    const int PEACEFUL = 0;
    const int EASY = 1;
    const int NORMAL = 2;
    const int HARD = 3;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::unique_ptr<ServerPlayer> player(float food, float health) {
        const NetworkIdentifier id("hunger", 1);
        auto player = std::make_unique<ServerPlayer>(id, 1);
        player->getAttributes() = ActorAttributes::createPlayerDefaults();
        player->setFood(food);
        player->setSaturation(0.0f);
        player->setHealth(health);
        return player;
    }

    void tick(ServerPlayer &player, int ticks, int difficulty, bool naturalRegeneration = true) {
        for (int index = 0; index < ticks; ++index)
            player.tickHunger(1, difficulty, naturalRegeneration);
    }

    void regeneration() {
        auto fed = player(20.0f, 10.0f);
        tick(*fed, 79, NORMAL);
        require(fed->getHealth() == 10.0f, "healed before 80 ticks");
        tick(*fed, 1, NORMAL);
        require(fed->getHealth() == 11.0f, "no heal at 80 ticks");

        auto offRule = player(20.0f, 10.0f);
        tick(*offRule, 400, NORMAL, false);
        require(offRule->getHealth() == 10.0f, "healed without naturalRegeneration");

        // whole hearts: 19.5 rounds up to the maximum
        auto almost = player(20.0f, 19.5f);
        tick(*almost, 400, NORMAL);
        require(almost->getHealth() == 19.5f, "healed a fraction short of full");
    }

    void timerRestarts() {
        auto fed = player(20.0f, 10.0f);
        tick(*fed, 60, NORMAL);
        fed->setFood(10.0f);
        tick(*fed, 1, NORMAL);
        require(fed->getFoodTickTimer() == 0, "timer kept running below 18 food");

        fed->setFood(20.0f);
        tick(*fed, 79, NORMAL);
        require(fed->getHealth() == 10.0f, "healed sooner than 80 ticks after reaching 18 food");
        tick(*fed, 1, NORMAL);
        require(fed->getHealth() == 11.0f, "no heal 80 ticks after reaching 18 food");
    }

    void starvation() {
        const struct {
            int mDifficulty;
            float mHealth;
            bool mStarves;
        } CASES[] = {{EASY, 11.0f, true},  {EASY, 10.5f, true},  {EASY, 10.0f, false}, {NORMAL, 2.0f, true},
                     {NORMAL, 1.5f, true}, {NORMAL, 1.0f, false}, {HARD, 1.0f, true}};

        for (const auto &entry: CASES) {
            auto starving = player(0.0f, entry.mHealth);
            tick(*starving, 79, entry.mDifficulty);
            require(!starving->consumeStarveDamage(), "starved before 80 ticks");
            tick(*starving, 1, entry.mDifficulty);
            require(starving->consumeStarveDamage() == entry.mStarves,
                    "difficulty " + std::to_string(entry.mDifficulty) + " at " + std::to_string(entry.mHealth)
                            + " health starved wrong");
        }
    }

    void peaceful() {
        auto hungry = player(10.0f, 10.0f);
        tick(*hungry, 10, PEACEFUL);
        require(hungry->getFood() == 11.0f, "peaceful gave no food in 10 ticks");
        tick(*hungry, 10, PEACEFUL);
        require(hungry->getFood() == 12.0f && hungry->getHealth() == 11.0f, "peaceful cadence is not 10 and 20 ticks");

        auto offRule = player(10.0f, 10.0f);
        tick(*offRule, 40, PEACEFUL, false);
        require(offRule->getHealth() == 10.0f && offRule->getFood() == 14.0f,
                "peaceful healed without naturalRegeneration or skipped food");

        auto empty = player(0.0f, 1.0f);
        tick(*empty, 400, PEACEFUL, false);
        require(!empty->consumeStarveDamage(), "starved on peaceful");
    }
}

int main() {
    try {
        regeneration();
        timerRestarts();
        starvation();
        peaceful();
        std::cout << "hunger checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
    return 0;
}
