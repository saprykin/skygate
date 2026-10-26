#pragma once

#include "AstronomicalEpoch.hpp"

namespace skygate::core {

struct AstronomicalEpochRange {
    AstronomicalEpoch start;
    AstronomicalEpoch end;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] bool contains(const AstronomicalEpoch& epoch) const noexcept;
    [[nodiscard]] bool contains(const AstronomicalEpochRange& other) const noexcept;

    friend bool operator==(const AstronomicalEpochRange&, const AstronomicalEpochRange&) = default;
};

}  // namespace skygate::core
