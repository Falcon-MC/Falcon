#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Types/GameRuleData.h"
#include "Protocol/Types/StartGameTypes.h"

#include <string>
#include <vector>

class GameRules {
public:
    struct Rule {
        std::string mName;
        GameRuleData::Type mType = GameRuleData::Type::Bool;
        bool mBoolValue = false;
        int32_t mIntValue = 0;
    };

    GameRules();

    bool getBool(const std::string &name) const;

    int32_t getInt(const std::string &name) const;

    bool setFromString(const std::string &name, const std::string &value);

    const Rule *find(const std::string &name) const;

    const std::vector<Rule> &getRules() const { return mRules; }

    std::vector<std::string> getNames() const;

    std::vector<GameRuleData> toNetwork() const;

    ChangedGameRuleData toChangedNetwork(const Rule &rule) const;

    Tag save() const;

    void load(const Tag &data);

private:
    Rule *_find(const std::string &name);

    void _addBool(const std::string &name, bool value);

    void _addInt(const std::string &name, int32_t value);

    std::vector<Rule> mRules;
};
