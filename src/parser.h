#pragma once

#include <string>
#include <string_view>
#include <optional>

#include <cstddef>
#include "timeutil.h"

class BasicParser {
    std::string                 source;
    std::string::const_iterator pointer;

    // reson to create local is_digit, is because the cctype one does also
    // include characters 'a'..'z' for hex nums
    static inline auto is_digit(char c) -> bool;

    static inline auto is_word(char c) -> bool;

public:
    explicit BasicParser(std::string source);

    // needed to check for '\n' and ' ' between tokens
    auto skip(char expected) -> bool;
    auto number() -> std::optional<std::size_t>;
    // FORMAT: HH [COLON] MM
    auto time_point() -> std::optional<timeutil::TimePoint>;
    // FORMAT: TIME_POINT [SPACE] TIME_POINT
    auto time_interval() -> std::optional<timeutil::TimeInterval>;
    auto word() -> std::optional<std::string_view>;
};
