#pragma once

#include <string>
#include <vector>

struct AllowListEntry {
    std::string mName;
    std::string mXuid;
    bool mIgnoresPlayerLimit = false;
};

class AllowList {
public:
    explicit AllowList(const std::string &path);

    // false when the file exists but does not parse
    bool reload();

    bool isAllowed(const std::string &name, const std::string &xuid);

    bool ignoresPlayerLimit(const std::string &name, const std::string &xuid);

    bool add(const std::string &name);

    bool remove(const std::string &name);

    bool isEmpty() const {
        return mEntries.empty();
    }

    std::vector<std::string> getNames() const;

    const std::vector<AllowListEntry> &getEntries() const {
        return mEntries;
    }

    bool contains(const std::string &name, const std::string &xuid);

    // false when an entry for that name or xuid exists
    bool addEntry(const std::string &name, const std::string &xuid, bool ignoresPlayerLimit);

    bool removeEntry(const std::string &name, const std::string &xuid);

    void clear();

private:
    std::vector<AllowListEntry>::iterator _findByName(const std::string &name);

    std::vector<AllowListEntry>::iterator _findPlayer(const std::string &name, const std::string &xuid);

    void _save() const;

    std::string mPath;
    std::vector<AllowListEntry> mEntries;
};
