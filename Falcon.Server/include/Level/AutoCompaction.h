#pragma once

class Level;

class AutoCompaction {
public:
    static constexpr int INTERVAL_SECONDS = 360;

    static void start(Level &level);

    static void stop();
};
