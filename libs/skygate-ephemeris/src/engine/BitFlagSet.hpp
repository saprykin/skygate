#pragma once

#include <cstdint>

namespace skygate::ephemeris {

// Holds a set of flags in one mask. Enumerators that already carry a mask
// value convert directly, so a named flag, a combination of named flags, and a
// plain integer mask all live in the same type. Enumerators that only store a
// bit position go through bit() before they reach the set.
template <typename Enum>
class BitFlagSet {
public:
    using Type = Enum;
    using Bits = std::uint32_t;

    constexpr BitFlagSet() noexcept = default;

    constexpr BitFlagSet(const Enum flag) noexcept : m_bits(static_cast<Bits>(flag)) {}

    explicit constexpr BitFlagSet(const Bits bits) noexcept : m_bits(bits) {}

    [[nodiscard]] static constexpr BitFlagSet fromBits(const Bits bits) noexcept
    {
        return BitFlagSet(bits);
    }

    [[nodiscard]] static constexpr Bits bit(const Enum position) noexcept
    {
        return Bits{1U} << static_cast<Bits>(position);
    }

    [[nodiscard]] constexpr Bits bits() const noexcept
    {
        return m_bits;
    }

    [[nodiscard]] constexpr bool hasAny() const noexcept
    {
        return m_bits != 0U;
    }

    [[nodiscard]] constexpr bool has(const BitFlagSet flags) const noexcept
    {
        return (m_bits & flags.m_bits) != 0U;
    }

    [[nodiscard]] constexpr bool has(const Bits bits) const noexcept
    {
        return (m_bits & bits) != 0U;
    }

    void add(const BitFlagSet flags) noexcept
    {
        m_bits |= flags.m_bits;
    }

    void add(const Bits bits) noexcept
    {
        m_bits |= bits;
    }

    void merge(const BitFlagSet other) noexcept
    {
        m_bits |= other.m_bits;
    }

    [[nodiscard]] constexpr BitFlagSet without(const BitFlagSet removedFlags) const noexcept
    {
        return fromBits(m_bits & ~removedFlags.m_bits);
    }

    [[nodiscard]] constexpr BitFlagSet operator|(const BitFlagSet rhs) const noexcept
    {
        return fromBits(m_bits | rhs.m_bits);
    }

    [[nodiscard]] constexpr BitFlagSet operator&(const BitFlagSet rhs) const noexcept
    {
        return fromBits(m_bits & rhs.m_bits);
    }

    constexpr BitFlagSet& operator|=(const BitFlagSet rhs) noexcept
    {
        merge(rhs);
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const BitFlagSet rhs) const noexcept
    {
        return m_bits == rhs.m_bits;
    }

    [[nodiscard]] constexpr bool operator!=(const BitFlagSet rhs) const noexcept
    {
        return m_bits != rhs.m_bits;
    }

    explicit constexpr operator Bits() const noexcept
    {
        return m_bits;
    }

private:
    Bits m_bits = 0U;
};

}  // namespace skygate::ephemeris
