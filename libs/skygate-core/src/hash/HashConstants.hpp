#pragma once

#include <cstdint>

namespace skygate::core {

// Constants for the 64-bit FNV-1a hash function.
class HashConstants final {
public:
    HashConstants() = delete;

    static constexpr std::uint64_t kFnv1a64OffsetBasis = 14'695'981'039'346'656'037ULL;
    static constexpr std::uint64_t kFnv1a64Prime = 1'099'511'628'211ULL;
};

}  // namespace skygate::core
