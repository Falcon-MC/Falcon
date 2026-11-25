#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

template <typename T>
class EventSignal {
public:
    using Handler = std::function<void(T &)>;

    uint32_t subscribe(Handler handler) {
        const uint32_t id = mNextId++;
        mEntries.push_back(Entry{id, std::move(handler)});
        return id;
    }

    void unsubscribe(uint32_t id) {
        for (auto it = mEntries.begin(); it != mEntries.end(); ++it) {
            if (it->mId == id) {
                mEntries.erase(it);
                return;
            }
        }
    }

    void emit(T &event) {
        if (mEntries.empty())
            return;

        const std::vector<Entry> snapshot = mEntries;
        for (const Entry &entry: snapshot)
            entry.mHandler(event);
    }

    bool hasSubscribers() const {
        return !mEntries.empty();
    }

    size_t subscriberCount() const {
        return mEntries.size();
    }

    void clear() {
        mEntries.clear();
    }

private:
    struct Entry {
        uint32_t mId;
        Handler mHandler;
    };

    std::vector<Entry> mEntries;
    uint32_t mNextId = 1;
};
