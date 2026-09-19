#pragma once

#include <string>
#include <vector>

struct BanEntry {
    std::string mName;
    std::string mCreationDate;
    std::string mSource;
    std::string mExpireDate;
    std::string mReason;
};

class BanList {
public:
    explicit BanList(const std::string &path);

    void reload();

    const BanEntry *find(const std::string &name) const;

    void add(const std::string &name, const std::string &reason, const std::string &source);

    bool remove(const std::string &name);

    std::vector<std::string> getNames() const;

private:
    static std::string _currentDate();

    void _save() const;

    std::string mPath;
    std::vector<BanEntry> mEntries;
};
