#pragma once

#include <string>
#include <unordered_map>

// english texts of the game, read from a vanilla resource pack the admin put in resource_packs: falcon
// ships none of them
class VanillaTexts {
public:
    static const VanillaTexts &getInstance();

    // the key itself when no pack has it, so the client can still translate it
    std::string translate(const std::string &key) const;

    bool isLoaded() const {
        return !mTexts.empty();
    }

    void loadFromString(const std::string &contents);

private:
    VanillaTexts();

    std::unordered_map<std::string, std::string> mTexts;
};
