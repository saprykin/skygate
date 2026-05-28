#pragma once

#include "CompressedDataInflateOptions.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

class CompressedDataInflater final {
public:
    enum class Format {
        Gzip,
        RawDeflate
    };

    [[nodiscard]] static std::optional<std::string>
    inflate(std::string_view compressedData, Format format, const CompressedDataInflateOptions& options = {});
};

}  // namespace skygate::ephemeris
