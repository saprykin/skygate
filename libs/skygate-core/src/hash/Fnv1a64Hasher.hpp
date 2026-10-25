#pragma once

#include <cstdint>
#include <string_view>

namespace skygate::core {

// 64-bit FNV-1a hashing. `hashFrom()` advances a state, starting from
// `HashConstants::kFnv1a64OffsetBasis` for a fresh hash. `combine()` folds an
// integral value into the state, and `appendComponent()` folds a string
// followed by a separator so a sequence of components differs from their
// concatenation.
class Fnv1a64Hasher final {
public:
    Fnv1a64Hasher() = delete;

    [[nodiscard]] static std::uint64_t hashFrom(std::uint64_t state, std::string_view value) noexcept;
    [[nodiscard]] static std::uint64_t combine(std::uint64_t state, std::uint64_t value) noexcept;
    [[nodiscard]] static std::uint64_t appendComponent(std::uint64_t state, std::string_view value) noexcept;
};

}  // namespace skygate::core
