#ifndef PARSE_ENV_INT_H
#define PARSE_ENV_INT_H

#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

inline std::optional<int> ParseEnvInt(std::string_view s)
{
    if (s.empty()) {
        return std::nullopt;
    }
    int value = 0;
    const char *first = s.data();
    const char *last = first + s.size();
    auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc() || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

#endif
