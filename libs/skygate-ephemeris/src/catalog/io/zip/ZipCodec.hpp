#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

class ZipCodec final {
public:
    [[nodiscard]] std::optional<std::string> extractFirstCsvEntry(std::string_view zipData) const;
};

}  // namespace skygate::ephemeris
