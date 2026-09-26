#pragma once

#include <string>
#include <vector>

class DamageCause {
public:
    static const char *findDeathMessageKey(const std::string &cause);

    static std::vector<std::string> getNames();

    static bool matches(const std::string &cause, const std::string &deathMessageKey);
};
