#include "ZipFieldReader.hpp"
#include "ZipBinaryUtilities.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace skygate::ephemeris {

ZipFieldReader::ZipFieldReader(const std::string_view data) noexcept : m_data(data) {}

std::uint16_t ZipFieldReader::le16(const std::size_t offset) noexcept
{
    const std::optional<std::uint16_t> value = ZipBinaryUtilities::readLe16(m_data, offset);
    if (!value.has_value()) {
        m_complete = false;
        return 0U;
    }

    return *value;
}

std::uint32_t ZipFieldReader::le32(const std::size_t offset) noexcept
{
    const std::optional<std::uint32_t> value = ZipBinaryUtilities::readLe32(m_data, offset);
    if (!value.has_value()) {
        m_complete = false;
        return 0U;
    }

    return *value;
}

bool ZipFieldReader::isComplete() const noexcept
{
    return m_complete;
}

}  // namespace skygate::ephemeris
