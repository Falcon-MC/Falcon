#pragma once

#include <stdexcept>
#include <string>

class BinaryDataException : public std::runtime_error {
public:
    explicit BinaryDataException(const std::string &message) : std::runtime_error(message) {
    }
};
