#pragma once

#include "Protocol/Types/AttributeData.h"

#include <vector>

class ActorAttributes {
public:
    ActorAttributes();

    static ActorAttributes createPlayerDefaults();

    static ActorAttributes createActorDefaults();

    void set(const std::string &name, float value);

    void addMaximum(const std::string &name, float amount);

    void setBaseMaximum(const std::string &name, float maximum);

    float get(const std::string &name) const;

    float getMinimum(const std::string &name) const;

    float getMaximum(const std::string &name) const;

    void setClamped(const std::string &name, float value);

    const std::vector<AttributeData> &getAll() const { return mAttributes; }

private:
    void _add(const std::string &name, float minimum, float maximum, float value);

    std::vector<AttributeData> mAttributes;
};
