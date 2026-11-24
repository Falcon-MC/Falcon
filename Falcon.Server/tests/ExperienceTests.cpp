#include "World/Actor/Player/ExperienceManager.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void totals() {
        ExperienceManager experience;
        for (int points = 0; points <= 100000; ++points) {
            experience.setCurrentTotalXp(points);
            require(experience.getCurrentTotalXp() == points,
                    "setting " + std::to_string(points) + " XP returned "
                            + std::to_string(experience.getCurrentTotalXp()));
        }
        for (int level = 1; level <= 1000; ++level) {
            const int threshold = ExperienceManager::getXpToReachLevel(level);
            experience.setCurrentTotalXp(threshold - 1);
            require(experience.getXpLevel() == level - 1, "a point below the threshold advanced the level");
            experience.setCurrentTotalXp(threshold);
            require(experience.getXpLevel() == level && experience.getXpProgress() == 0.0f,
                    "a level threshold left a partial bar");
        }
        experience.setCurrentTotalXp(-1);
        require(experience.getCurrentTotalXp() == 0, "negative XP was not clamped to zero");
    }

    void gains() {
        ExperienceManager experience;
        for (int points = 1; points <= 100000; ++points) {
            experience.addXp(1);
            require(experience.getCurrentTotalXp() == points,
                    "single-point gains drifted at " + std::to_string(points));
        }
        require(experience.getLifetimeTotalXp() == 100000, "lifetime XP did not count the gains");
        experience.subtractXp(98453);
        require(experience.getCurrentTotalXp() == 1547, "subtracting XP changed the remaining points");
        experience.addXp(1);
        require(experience.getCurrentTotalXp() == 1548, "adding one XP awarded more than one point");
    }

    void progress() {
        ExperienceManager experience;
        experience.setCurrentTotalXp(1);
        require(std::fabs(experience.getXpProgress() - 1.0f / 7.0f) < 0.000001f,
                "one XP did not fill one seventh of the first bar");
        experience.setXpAndProgress(31, 0.5f);
        require(experience.getRemainderXp() == 61, "half of a 121-point bar did not round to 61 XP");
        experience.setCurrentTotalXp(1547);
        require(experience.getXpLevel() == 31
                        && std::fabs(experience.getXpProgress() - 40.0f / 121.0f) < 0.000001f,
                "the progress was taken from the fractional level instead of the remaining points");
    }
}

int main() {
    try {
        totals();
        gains();
        progress();
    } catch (const std::exception &error) {
        std::clog << "Experience regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
