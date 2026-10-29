#pragma once

#include <cstdint>
#include <string>

class ServerActor;

struct MolangValue {
    double mNumber = 0.0;
    std::string mString;
    bool mIsString = false;

    static MolangValue ofNumber(double number);

    static MolangValue ofString(std::string string);

    bool isTrue() const;
};

struct MolangContext {
    bool mLastHitByPlayer = false;
    int32_t mPlayerLevel = 0;
};

class Molang {
public:
    static MolangValue evaluate(const std::string &expression, const ServerActor &actor);

    static MolangValue evaluate(const std::string &expression, const ServerActor &actor, const MolangContext &context);
};
