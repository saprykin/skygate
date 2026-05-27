#pragma once

namespace skygate::ephemeris {

template <typename FlagSet>
class BitFlagSetOperations {
public:
    [[nodiscard]] static constexpr bool hasFlag(const FlagSet flags, const FlagSet flag) noexcept
    {
        return (flags & flag) != FlagSet{};
    }

    [[nodiscard]] static constexpr FlagSet withoutFlags(const FlagSet flags, const FlagSet removedFlags) noexcept
    {
        return FlagSet(static_cast<decltype(flags.bits())>(flags.bits() & ~removedFlags.bits()));
    }
};

}  // namespace skygate::ephemeris
