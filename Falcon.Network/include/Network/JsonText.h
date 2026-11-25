#pragma once

#include "Core/Json/Json.h"

#include <string>
#include <vector>

namespace JsonText {

    std::string quote(const std::string &value);

    bool findMember(const std::string &objectText, const std::string &key, bool ignoreCase, std::string &outRawValue);

    bool splitArray(const std::string &arrayText, std::vector<std::string> &outElements);

    bool readString(const std::string &rawValue, std::string &outValue);

    const json::Value *getMember(const json::Value &object, const std::string &key, bool ignoreCase);

}
