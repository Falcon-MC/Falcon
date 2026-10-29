#pragma once

#include <string>
#include <vector>

class BlockPlacementFilterDescription {
public:
    bool allows(const std::string &identifier) const {
        if (mAllowedIdentifiers.empty())
            return true;
        for (const std::string &allowed : mAllowedIdentifiers) {
            if (allowed == identifier)
                return true;
        }
        return false;
    }

    std::vector<std::string> mAllowedIdentifiers;
};
