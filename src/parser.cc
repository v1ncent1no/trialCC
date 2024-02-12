#include "parser.h"

inline auto BasicParser::is_digit(const char c) -> bool
{
    return c >= '0' && c <= '9';
}

inline auto BasicParser::is_word(const char c) -> bool
{
    return (c >= 'a' && c <= 'z') || is_digit(c) || c == '_' || c == '-';
}

BasicParser::BasicParser(std::string source)
    : source(std::move(source))
    , pointer(this->source.cbegin())
{
}

// needed to check for '\n' and ' ' between tokens
auto BasicParser::skip(const char expected) -> bool
{
    bool result = *pointer == expected;
    ++pointer;

    return result;
}

auto BasicParser::number() -> std::optional<std::size_t>
{
    std::optional<std::size_t> result {};

    std::size_t n { 0 };
    while (pointer != source.cend() && is_digit(*pointer))
        result = n = n * 10 + (*pointer++ - '0');

    return result;
}

// FORMAT: HH [COLON] MM
auto BasicParser::time_point() -> std::optional<timeutil::TimePoint>
{
    const auto hours = number();
    if (!hours.has_value()) {
        return std::nullopt;
    }

    if (!skip(':')) {
        return std::nullopt;
    }

    const auto minutes = number();
    if (!minutes.has_value()) {
        return std::nullopt;
    }

    return timeutil::make_time_point(hours.value(), minutes.value());
}

// FORMAT: TIME_POINT [SPACE] TIME_POINT
auto BasicParser::time_interval() -> std::optional<timeutil::TimeInterval>
{
    const auto begin = time_point();
    if (!begin.has_value()) {
        return std::nullopt;
    }

    if (!skip(' ')) {
        return std::nullopt;
    }

    const auto end = time_point();
    if (!end.has_value()) {
        return std::nullopt;
    }

    return timeutil::TimeInterval { begin.value(), end.value() };
}

auto BasicParser::word() -> std::optional<std::string_view>
{
    const auto begin = pointer;

    while (pointer != source.cend() && is_word(*pointer))
        pointer++;

    if (begin == pointer)
        return std::nullopt;

    return std::string_view { begin, pointer };
}
