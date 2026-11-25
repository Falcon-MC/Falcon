#pragma once

#include <string>
#include <vector>

class ActorDamageSource;
enum class Difficulty : int;

class DamageCause {
public:
    static const char *findDeathMessageKey(const std::string &cause);

    static std::vector<std::string> getNames();

    static bool matches(const std::string &cause, const std::string &deathMessageKey);

    static bool matches(const std::string &cause, const ActorDamageSource &source);

    // nothing on peaceful, half plus one above 2 on easy, half again on hard
    static float scaleForDifficulty(float damage, Difficulty difficulty);
};
