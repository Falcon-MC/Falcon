#pragma once

class Level;

class AutoCompaction {
public:
    static void start(Level &level, int intervalSeconds);

    static void stop();
};
