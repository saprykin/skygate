#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

// Sequential little-endian reader over one ZIP record.
//
// The directory reader and the entry extractor both read a fixed set of offsets
// and reject the record as a whole when any field falls outside the payload.
// This reader records the first out-of-range read and returns an empty field
// for it, so a caller checks isComplete() once per record instead of threading
// a has_value() check through every field.
class ZipFieldReader final {
public:
    explicit ZipFieldReader(std::string_view data) noexcept;

    // Reads one field at an absolute payload offset. A read outside the payload
    // marks the reader incomplete and yields zero.
    [[nodiscard]] std::uint16_t le16(std::size_t offset) noexcept;
    [[nodiscard]] std::uint32_t le32(std::size_t offset) noexcept;

    // True while every field read so far was inside the payload.
    [[nodiscard]] bool isComplete() const noexcept;

private:
    std::string_view m_data;
    bool m_complete = true;
};

}  // namespace skygate::ephemeris
