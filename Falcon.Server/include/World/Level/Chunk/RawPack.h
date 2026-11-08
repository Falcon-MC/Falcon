#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// read back by the same process only: host-endian integers and pointers to interned block states
namespace RawPack {
    template<typename T>
    void put(std::string &out, const T &value) {
        out.append((const char *) &value, sizeof(T));
    }

    template<typename T>
    void putVector(std::string &out, const std::vector<T> &values) {
        put(out, (uint32_t) values.size());
        if (!values.empty())
            out.append((const char *) values.data(), values.size() * sizeof(T));
    }

    class Reader {
    public:
        Reader(const char *data, size_t size) : mCursor(data), mEnd(data + size) {}

        template<typename T>
        bool get(T &value) {
            if ((size_t) (mEnd - mCursor) < sizeof(T))
                return false;

            std::memcpy(&value, mCursor, sizeof(T));
            mCursor += sizeof(T);
            return true;
        }

        template<typename T>
        bool getVector(std::vector<T> &values) {
            uint32_t count = 0;
            if (!get(count) || (size_t) (mEnd - mCursor) / sizeof(T) < count)
                return false;

            values.resize(count);
            if (count != 0)
                std::memcpy(values.data(), mCursor, count * sizeof(T));
            mCursor += count * sizeof(T);
            return true;
        }

        bool atEnd() const {
            return mCursor == mEnd;
        }

    private:
        const char *mCursor;
        const char *mEnd;
    };
}
