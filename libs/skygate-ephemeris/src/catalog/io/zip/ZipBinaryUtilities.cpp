#include "ZipBinaryUtilities.hpp"

#include <QtEndian>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

std::optional<std::uint16_t> ZipBinaryUtilities::readLe16(const std::string_view data, const std::size_t offset)
{
    if (offset + 2U > data.size()) {
        return std::nullopt;
    }

    return qFromLittleEndian<std::uint16_t>(data.data() + offset);
}

std::optional<std::uint32_t> ZipBinaryUtilities::readLe32(const std::string_view data, const std::size_t offset)
{
    if (offset + 4U > data.size()) {
        return std::nullopt;
    }

    return qFromLittleEndian<std::uint32_t>(data.data() + offset);
}

}  // namespace skygate::ephemeris
