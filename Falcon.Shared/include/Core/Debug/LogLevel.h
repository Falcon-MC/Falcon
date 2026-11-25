#pragma once

enum class LogLevel : unsigned char {
    Verbose = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4
};

const char *toString(LogLevel level);
