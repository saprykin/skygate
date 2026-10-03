#pragma once

#include "EphemerisEngineDescriptor.hpp"

#include <cstddef>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisEngineDescriptorRegistry final {
public:
    EphemerisEngineDescriptorRegistry() = delete;

    [[nodiscard]] static const EphemerisEngineDescriptor& defaultDescriptor() noexcept;
    [[nodiscard]] static const EphemerisEngineDescriptor& descriptorAt(std::size_t index) noexcept;
    [[nodiscard]] static std::size_t count() noexcept;
    [[nodiscard]] static std::size_t indexOf(EphemerisEngineKind::Type kind) noexcept;
    [[nodiscard]] static const EphemerisEngineDescriptor* findById(std::string_view id) noexcept;
    [[nodiscard]] static const EphemerisEngineDescriptor* findByKind(EphemerisEngineKind::Type kind) noexcept;
};

}  // namespace skygate::ephemeris
