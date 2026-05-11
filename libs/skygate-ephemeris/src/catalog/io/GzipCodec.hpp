#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

class GzipCodec final {
public:
    [[nodiscard]] std::optional<std::string> gunzip(std::string_view gzipData) const;
};

}  // namespace skygate::ephemeris
