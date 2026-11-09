#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// one operator per line: the name, then a tab and the xuid once the account is known
class OpList {
public:
    explicit OpList(const std::string &path);

    bool isOp(const std::string &name) const;

    // a name whose xuid is on record belongs to that account only; an unsigned login has none to compare
    bool isOp(const std::string &name, const std::string &xuid) const;

    // records the account behind an operator name the first time it logs in signed
    void bindXuid(const std::string &name, const std::string &xuid);

    std::vector<std::string> getNames() const;

    void addOp(const std::string &name);

    void removeOp(const std::string &name);

    void reload();

private:
    void _save() const;

    std::string mPath;
    std::unordered_map<std::string, std::string> mNames;
};
