#pragma once

#include "skygate/core/Types.hpp"

namespace skygate::core {

class ITimeSource {
public:
    virtual ~ITimeSource() = default;
    [[nodiscard]] virtual UtcTimePoint nowUtc() const noexcept = 0;
};

}  // namespace skygate::core
