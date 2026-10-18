#include "Fnv1a64Hasher.hpp"
#include "HashConstants.hpp"

namespace skygate::core {
namespace {

constexpr std::uint64_t kComponentSeparator = 0xffU;

}  // namespace

std::uint64_t Fnv1a64Hasher::hash(const std::string_view value) noexcept
{
    return hashFrom(HashConstants::kFnv1a64OffsetBasis, value);
}

std::uint64_t Fnv1a64Hasher::hashFrom(std::uint64_t state, const std::string_view value) noexcept
{
    for (const char character : value) {
        state ^= static_cast<unsigned char>(character);
        state *= HashConstants::kFnv1a64Prime;
    }
    return state;
}

std::uint64_t Fnv1a64Hasher::combine(std::uint64_t state, const std::uint64_t value) noexcept
{
    return (state ^ value) * HashConstants::kFnv1a64Prime;
}

std::uint64_t Fnv1a64Hasher::appendComponent(std::uint64_t state, const std::string_view value) noexcept
{
    return combine(hashFrom(state, value), kComponentSeparator);
}

}  // namespace skygate::core
