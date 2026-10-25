#include "StringUtilities.hpp"
#include "AsciiText.hpp"

#include <QByteArray>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <utility>

namespace skygate::ephemeris {
namespace {

template <typename SameValue>
bool appendUniqueValue(std::vector<std::string>& values, std::string value, SameValue sameValue)
{
    if (value.empty()) {
        return false;
    }

    const bool alreadyPresent =
        std::any_of(values.begin(), values.end(), [&value, &sameValue](const std::string& existing) {
            return sameValue(existing, value);
        });
    if (alreadyPresent) {
        return false;
    }

    values.push_back(std::move(value));
    return true;
}

}  // namespace

bool StringUtilities::isAsciiWhitespace(const char character) noexcept
{
    return skygate::core::AsciiText::isWhitespace(character);
}

bool StringUtilities::isAsciiDigit(const char character) noexcept
{
    return skygate::core::AsciiText::isDigit(character);
}

bool StringUtilities::isAsciiLetter(const char character) noexcept
{
    return skygate::core::AsciiText::isLetter(character);
}

char StringUtilities::toLowerAscii(const char character) noexcept
{
    return skygate::core::AsciiText::toLower(character);
}

std::string StringUtilities::toLowerAscii(const std::string_view value)
{
    return skygate::core::AsciiText::toLower(value);
}

std::string_view StringUtilities::trimAsciiWhitespace(const std::string_view value) noexcept
{
    return skygate::core::AsciiText::trimWhitespace(value);
}

std::string_view StringUtilities::takeLine(std::string_view& remaining) noexcept
{
    const std::size_t newline = remaining.find('\n');
    std::string_view line = newline == std::string_view::npos ? remaining : remaining.substr(0U, newline);
    remaining = newline == std::string_view::npos ? std::string_view{} : remaining.substr(newline + 1U);
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1U);
    }
    return line;
}

bool StringUtilities::equalsIgnoreAsciiCase(const std::string_view lhs, const std::string_view rhs) noexcept
{
    return skygate::core::AsciiText::equalsIgnoreCase(lhs, rhs);
}

bool StringUtilities::containsIgnoreAsciiCase(const std::string_view value, const std::string_view token)
{
    return toLowerAscii(value).find(toLowerAscii(token)) != std::string::npos;
}

std::string StringUtilities::toUtf8(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QString StringUtilities::fromUtf8(const std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::string StringUtilities::normalizedLookupKey(const std::string_view value)
{
    return skygate::core::AsciiText::normalizedLookupKey(value);
}

std::string StringUtilities::normalizedAlnumKey(const std::string_view value)
{
    std::string key;
    key.reserve(value.size());
    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0) {
            key.push_back(toLowerAscii(static_cast<char>(character)));
        }
    }
    return key;
}

QString StringUtilities::normalizedLabelText(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    text.replace('_', ' ');
    text.replace('-', ' ');
    return text.simplified();
}

std::vector<std::string_view> StringUtilities::splitView(const std::string_view text, const char delimiter)
{
    std::vector<std::string_view> tokens;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const std::size_t next = text.find(delimiter, cursor);
        const std::size_t end = (next == std::string_view::npos) ? text.size() : next;
        const std::string_view token = text.substr(cursor, end - cursor);
        if (!token.empty()) {
            tokens.push_back(token);
        }
        if (next == std::string_view::npos) {
            break;
        }
        cursor = next + 1U;
    }
    return tokens;
}

bool StringUtilities::appendUnique(std::vector<std::string>& values, std::string value)
{
    return appendUniqueValue(values, std::move(value), [](const std::string& existing, const std::string& candidate) {
        return existing == candidate;
    });
}

bool StringUtilities::appendUniqueIgnoreAsciiCase(std::vector<std::string>& values, std::string value)
{
    return appendUniqueValue(values, std::move(value), [](const std::string& existing, const std::string& candidate) {
        return skygate::core::AsciiText::equalsIgnoreCase(existing, candidate);
    });
}

}  // namespace skygate::ephemeris
