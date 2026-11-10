#pragma once

#include <string>
#include <vector>

struct BanEntry {
    std::string mName;
    // account behind the name, once seen: a renamed gamertag stays banned
    std::string mXuid;
    std::string mCreationDate;
    std::string mSource;
    std::string mExpireDate;
    std::string mReason;
};

class BanList {
public:
    explicit BanList(const std::string &path);

    void reload();

    const BanEntry *find(const std::string &name, const std::string &xuid = std::string()) const;

    void add(const std::string &name, const std::string &reason, const std::string &source,
             const std::string &xuid = std::string());

    // records the account behind a banned name the first time it shows up signed
    void bindXuid(const std::string &name, const std::string &xuid);

    bool remove(const std::string &name);

    std::vector<std::string> getNames() const;

private:
    static std::string _currentDate();

    void _save() const;

    std::string mPath;
    std::vector<BanEntry> mEntries;
};
