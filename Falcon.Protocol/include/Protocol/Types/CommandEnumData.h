#pragma once

#include <string>
#include <vector>

class CommandEnumData {
public:
    std::string mName;
    std::vector<std::string> mValues;
    bool mIsSoft = false;
};
