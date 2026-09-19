#include "Level/GameRules.h"

#include <algorithm>
#include <cctype>

namespace {
    std::string toLower(const std::string &value) {
        std::string result = value;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
            return (char) std::tolower(character);
        });
        return result;
    }

    bool parseBool(const std::string &value, bool &result) {
        const std::string lowered = toLower(value);

        if (lowered == "true" || lowered == "1") {
            result = true;
            return true;
        }

        if (lowered == "false" || lowered == "0") {
            result = false;
            return true;
        }

        return false;
    }
}

GameRules::GameRules() {
    _addBool("commandblocksenabled", true);
    _addBool("commandblockoutput", true);
    _addBool("dodaylightcycle", true);
    _addBool("doentitydrops", true);
    _addBool("dofiretick", true);
    _addBool("doinsomnia", true);
    _addBool("doimmediaterespawn", false);
    _addBool("domobloot", true);
    _addBool("domobspawning", true);
    _addBool("dotiledrops", true);
    _addBool("doweathercycle", true);
    _addBool("drowningdamage", true);
    _addBool("falldamage", true);
    _addBool("firedamage", true);
    _addBool("freezedamage", true);
    _addInt("functioncommandlimit", 10000);
    _addBool("keepinventory", false);
    _addInt("maxcommandchainlength", 65535);
    _addBool("mobgriefing", true);
    _addBool("naturalregeneration", true);
    _addBool("pvp", true);
    _addInt("playerwaypoints", 1);
    _addInt("randomtickspeed", 1);
    _addBool("sendcommandfeedback", true);
    _addBool("showcoordinates", true);
    _addBool("showdeathmessages", true);
    _addInt("spawnradius", 5);
    _addBool("tntexplodes", true);
    _addBool("projectilescanbreakblocks", true);
    _addBool("tntexplosiondropdecay", true);
    _addBool("showtags", true);
    _addBool("experimentalgameplay", true);
    _addInt("playerssleepingpercentage", 100);
    _addBool("dolimitedcrafting", false);
    _addBool("respawnblocksexplode", true);
    _addBool("showbordereffect", true);
    _addBool("showdaysplayed", false);
    _addBool("showrecipemessages", true);
    _addBool("locatorbar", true);
    _addBool("recipesunlock", false);
}

void GameRules::_addBool(const std::string &name, bool value) {
    Rule rule;
    rule.mName = name;
    rule.mType = GameRuleData::Type::Bool;
    rule.mBoolValue = value;
    mRules.push_back(rule);
}

void GameRules::_addInt(const std::string &name, int32_t value) {
    Rule rule;
    rule.mName = name;
    rule.mType = GameRuleData::Type::Int;
    rule.mIntValue = value;
    mRules.push_back(rule);
}

const GameRules::Rule *GameRules::find(const std::string &name) const {
    const std::string lowered = toLower(name);

    for (const Rule &rule: mRules) {
        if (rule.mName == lowered)
            return &rule;
    }

    return nullptr;
}

GameRules::Rule *GameRules::_find(const std::string &name) {
    const std::string lowered = toLower(name);

    for (Rule &rule: mRules) {
        if (rule.mName == lowered)
            return &rule;
    }

    return nullptr;
}

bool GameRules::getBool(const std::string &name) const {
    const Rule *rule = find(name);
    return rule != nullptr && rule->mType == GameRuleData::Type::Bool && rule->mBoolValue;
}

int32_t GameRules::getInt(const std::string &name) const {
    const Rule *rule = find(name);
    return rule == nullptr || rule->mType != GameRuleData::Type::Int ? 0 : rule->mIntValue;
}

bool GameRules::setFromString(const std::string &name, const std::string &value) {
    Rule *rule = _find(name);
    if (rule == nullptr)
        return false;

    if (rule->mType == GameRuleData::Type::Bool)
        return parseBool(value, rule->mBoolValue);

    try {
        rule->mIntValue = (int32_t) std::stol(value);
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

std::vector<std::string> GameRules::getNames() const {
    std::vector<std::string> names;
    names.reserve(mRules.size());

    for (const Rule &rule: mRules)
        names.push_back(rule.mName);

    return names;
}

std::vector<GameRuleData> GameRules::toNetwork() const {
    std::vector<GameRuleData> data;
    data.reserve(mRules.size());

    for (const Rule &rule: mRules) {
        if (rule.mType == GameRuleData::Type::Bool)
            data.push_back(GameRuleData::ofBool(rule.mName, rule.mBoolValue));
        else
            data.push_back(GameRuleData::ofInt(rule.mName, rule.mIntValue));
    }

    return data;
}

ChangedGameRuleData GameRules::toChangedNetwork(const Rule &rule) const {
    ChangedGameRuleData data;
    data.mName = rule.mName;
    data.mEditable = true;

    if (rule.mType == GameRuleData::Type::Bool) {
        data.mType = ChangedGameRuleType::Bool;
        data.mBoolValue = rule.mBoolValue;
    } else {
        data.mType = ChangedGameRuleType::Int;
        data.mIntValue = rule.mIntValue;
    }

    return data;
}

Tag GameRules::save() const {
    Tag data = Tag::ofCompound();

    for (const Rule &rule: mRules) {
        if (rule.mType == GameRuleData::Type::Bool)
            data.putByte(rule.mName, rule.mBoolValue ? 1 : 0);
        else
            data.putInt(rule.mName, rule.mIntValue);
    }

    return data;
}

void GameRules::load(const Tag &data) {
    if (!data.isCompound())
        return;

    for (Rule &rule: mRules) {
        const Tag *stored = data.get(rule.mName);
        if (stored == nullptr)
            continue;

        if (rule.mType == GameRuleData::Type::Bool)
            rule.mBoolValue = stored->asByte() != 0;
        else
            rule.mIntValue = stored->asInt();
    }
}
