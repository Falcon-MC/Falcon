#pragma once

#include <cstdint>
#include <string>

enum class ScorerType {
    Invalid,
    Player,
    Entity,
    Fake
};

class ScoreInfoEntry {
public:
    int64_t mScoreboardId = 0;
    std::string mObjectiveId;
    int32_t mScore = 0;
    ScorerType mType = ScorerType::Invalid;
    std::string mName;
    int64_t mActorId = -1;
};
