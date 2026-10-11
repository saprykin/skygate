#pragma once

#include <cstdint>

namespace skygate::ephemeris {

class BitFlagSetOperations final {
public:
    BitFlagSetOperations() = delete;

    template <typename FlagSet>
    [[nodiscard]] static constexpr bool hasFlag(const FlagSet flags, const FlagSet flag) noexcept
    {
        return (flags & flag) != FlagSet{};
    }

    template <typename FlagSet>
    [[nodiscard]] static constexpr FlagSet withoutFlags(const FlagSet flags, const FlagSet removedFlags) noexcept
    {
        return FlagSet(static_cast<decltype(flags.bits())>(flags.bits() & ~removedFlags.bits()));
    }

    template <typename Enum>
    [[nodiscard]] static constexpr std::uint32_t bit(const Enum value) noexcept
    {
        return 1U << static_cast<std::uint8_t>(value);
    }
};

}  // namespace skygate::ephemeris
