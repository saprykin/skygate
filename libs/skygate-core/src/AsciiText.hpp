#pragma once

#include <cstddef>
#include <string_view>

namespace skygate::core {

// ASCII text primitives shared by codecs and parsers. The predicates never
// consult the C library locale, so stored and imported text reads the same way
// in every process.
class AsciiText final {
public:
    AsciiText() = delete;

    [[nodiscard]] static constexpr bool isWhitespace(const char character) noexcept
    {
        return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\f'
               || character == '\v';
    }

    [[nodiscard]] static constexpr bool isDigit(const char character) noexcept
    {
        return character >= '0' && character <= '9';
    }

    [[nodiscard]] static constexpr bool isLetter(const char character) noexcept
    {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
    }

    [[nodiscard]] static constexpr char toLower(const char character) noexcept
    {
        return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
    }

    [[nodiscard]] static constexpr std::string_view trimWhitespace(const std::string_view text) noexcept
    {
        std::size_t begin = 0;
        while (begin < text.size() && isWhitespace(text[begin])) {
            ++begin;
        }

        std::size_t end = text.size();
        while (end > begin && isWhitespace(text[end - 1])) {
            --end;
        }

        return text.substr(begin, end - begin);
    }

    [[nodiscard]] static constexpr bool
    equalsIgnoreCase(const std::string_view lhs, const std::string_view rhs) noexcept
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        for (std::size_t index = 0; index < lhs.size(); ++index) {
            if (toLower(lhs[index]) != toLower(rhs[index])) {
                return false;
            }
        }

        return true;
    }
};

}  // namespace skygate::core
