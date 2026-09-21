#pragma once

#include <string>

class Gzip {
public:
    static bool decompress(const std::string &input, std::string &output);
};
