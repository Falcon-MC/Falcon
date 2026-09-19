#pragma once

#include <string>
#include <unordered_set>
#include <vector>

class OpList {
public:
    explicit OpList(const std::string &path);

    bool isOp(const std::string &name) const;

    std::vector<std::string> getNames() const;

    void addOp(const std::string &name);

    void removeOp(const std::string &name);

    void reload();

private:
    void _save() const;

    std::string mPath;
    std::unordered_set<std::string> mNames;
};
