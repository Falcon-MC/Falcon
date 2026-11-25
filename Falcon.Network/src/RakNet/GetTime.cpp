#include "RakNet/GetTime.h"

#include <chrono>

namespace RakNet {

    static std::chrono::steady_clock::time_point GetStartTime() {
        static const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        return start;
    }

    TimeUS GetTimeUS() {
        const auto elapsed = std::chrono::steady_clock::now() - GetStartTime();
        return (TimeUS) std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    }

    TimeMS GetTimeMS() {
        return (TimeMS) (GetTimeUS() / 1000);
    }

    Time GetTime() {
        return (Time) (GetTimeUS() / 1000);
    }

}
