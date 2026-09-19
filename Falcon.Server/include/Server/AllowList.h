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

    void reload();

    bool isAllowed(const std::string &name, const std::string &xuid);

    bool add(const std::string &name);

    bool remove(const std::string &name);

    bool isEmpty() const {
        return mEntries.empty();
    }

    std::vector<std::string> getNames() const;

private:
    static std::string _toLowerCase(const std::string &value);

    static std::string _escape(const std::string &value);

    std::vector<AllowListEntry>::iterator _findByName(const std::string &name);

    void _save() const;

    std::string mPath;
    std::vector<AllowListEntry> mEntries;
};
