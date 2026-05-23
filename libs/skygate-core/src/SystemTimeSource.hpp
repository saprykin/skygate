#pragma once

#include "ITimeSource.hpp"

namespace skygate::core {

class SystemTimeSource final : public ITimeSource {
public:
    [[nodiscard]] UtcTimePoint nowUtc() const noexcept override;
};

}  // namespace skygate::core
