#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

enum class CompressedDataFormat {
    Gzip,
    RawDeflate
};

struct CompressedDataInflateOptions final {
    std::size_t expectedOutputBytes = 0;
    bool allowEmptyOutput = false;
};

class CompressedDataInflater final {
public:
    [[nodiscard]] static std::optional<std::string> inflate(
        std::string_view compressedData,
        CompressedDataFormat format,
        const CompressedDataInflateOptions& options = {}
    );
};

}  // namespace skygate::ephemeris
