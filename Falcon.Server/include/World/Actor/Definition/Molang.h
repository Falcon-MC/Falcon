#pragma once

#include <cstdint>
#include <string>

#include "Core/NBT/Tag.h"

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
    // read by query.block_state
    const Tag *mBlockStates = nullptr;
};

class Molang {
public:
    static MolangValue evaluate(const std::string &expression, const ServerActor &actor);

    static MolangValue evaluate(const std::string &expression, const ServerActor &actor, const MolangContext &context);

    // a block permutation condition: no actor, only query.block_state
    static MolangValue evaluateForBlock(const std::string &expression, const Tag &states);
};
