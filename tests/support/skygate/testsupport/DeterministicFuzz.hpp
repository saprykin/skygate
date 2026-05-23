#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace skygate::testsupport {

class DeterministicRng final {
public:
    explicit constexpr DeterministicRng(const std::uint64_t seed) noexcept :
        m_state(seed == 0U ? 0x9e3779b97f4a7c15ULL : seed)
    {
    }

    [[nodiscard]] std::uint32_t nextU32() noexcept
    {
        m_state = (m_state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(m_state >> 32U);
    }

    [[nodiscard]] std::size_t index(const std::size_t upperBound) noexcept
    {
        if (upperBound == 0U) {
            return 0U;
        }
        return static_cast<std::size_t>(nextU32()) % upperBound;
    }

    [[nodiscard]] int intInRange(const int minValue, const int maxValue) noexcept
    {
        if (maxValue <= minValue) {
            return minValue;
        }
        const auto range = static_cast<std::uint32_t>(maxValue - minValue + 1);
        return minValue + static_cast<int>(nextU32() % range);
    }

    [[nodiscard]] double realInRange(const double minValue, const double maxValue) noexcept
    {
        const double unit =
            static_cast<double>(nextU32()) / static_cast<double>(UINT32_MAX);
        return minValue + ((maxValue - minValue) * unit);
    }

    [[nodiscard]] bool chance(
        const std::uint32_t numerator,
        const std::uint32_t denominator
    ) noexcept
    {
        if (denominator == 0U) {
            return false;
        }
        return (nextU32() % denominator) < numerator;
    }

    [[nodiscard]] char pickChar(const std::string_view alphabet) noexcept
    {
        if (alphabet.empty()) {
            return '\0';
        }
        return alphabet[index(alphabet.size())];
    }

    [[nodiscard]] std::string token(
        const std::size_t length,
        const std::string_view alphabet
    ) noexcept
    {
        std::string value;
        value.reserve(length);
        for (std::size_t i = 0; i < length; ++i) {
            value.push_back(pickChar(alphabet));
        }
        return value;
    }

private:
    std::uint64_t m_state = 0U;
};

}  // namespace skygate::testsupport
