#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

class ZipArchiveReader final {
public:
    [[nodiscard]] static std::optional<std::string> extractFirstCsvEntry(std::string_view zipData);
};

}  // namespace skygate::ephemeris
