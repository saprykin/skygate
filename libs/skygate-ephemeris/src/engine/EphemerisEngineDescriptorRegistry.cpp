#include "EphemerisEngineDescriptorRegistry.hpp"
#include "EphemerisCorrectionFlags.hpp"

#include <array>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] EphemerisEngineOptions simpleDefaultOptions() noexcept
{
    EphemerisEngineOptions options;
    options.setEngineKind(EphemerisEngineKind::Type::Simple);
    options.setCorrectionFlags(EphemerisCorrectionFlags::noCorrections());
    options.setEnableAtmosphericRefraction(false);
    return options;
}

[[nodiscard]] EphemerisEngineOptions highPrecisionDefaultOptions() noexcept
{
    EphemerisEngineOptions options;
    options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(EphemerisCorrectionFlags::apparentTopocentric());
    options.setEnableAtmosphericRefraction(true);
    return options;
}

[[nodiscard]] const std::array<EphemerisEngineDescriptor, 2>& descriptors() noexcept
{
    static const std::array<EphemerisEngineDescriptor, 2> kDescriptors{{
        EphemerisEngineDescriptor{
            .id = "simple",
            .kind = EphemerisEngineKind::Type::Simple,
            .displayName = "Simple",
            .defaultOptions = simpleDefaultOptions(),
            .supportsCorrections = false,
            .supportsAtmosphereSettings = false,
        },
        EphemerisEngineDescriptor{
            .id = "highPrecision",
            .kind = EphemerisEngineKind::Type::HighPrecision,
            .displayName = "High precision",
            .defaultOptions = highPrecisionDefaultOptions(),
            .supportsCorrections = true,
            .supportsAtmosphereSettings = true,
        },
    }};
    return kDescriptors;
}

}  // namespace

const EphemerisEngineDescriptor& EphemerisEngineDescriptorRegistry::defaultDescriptor() noexcept
{
    return descriptors().front();
}

const EphemerisEngineDescriptor& EphemerisEngineDescriptorRegistry::descriptorAt(const std::size_t index) noexcept
{
    if (index >= descriptors().size()) {
        return defaultDescriptor();
    }
    return descriptors()[index];
}

std::size_t EphemerisEngineDescriptorRegistry::count() noexcept
{
    return descriptors().size();
}

std::size_t EphemerisEngineDescriptorRegistry::indexOf(const EphemerisEngineKind::Type kind) noexcept
{
    const auto& all = descriptors();
    for (std::size_t index = 0; index < all.size(); ++index) {
        if (all[index].kind == kind) {
            return index;
        }
    }
    return 0U;
}

const EphemerisEngineDescriptor* EphemerisEngineDescriptorRegistry::findById(const std::string_view id) noexcept
{
    const auto& all = descriptors();
    for (const EphemerisEngineDescriptor& descriptor : all) {
        if (descriptor.id == id) {
            return &descriptor;
        }
    }
    return nullptr;
}

const EphemerisEngineDescriptor*
EphemerisEngineDescriptorRegistry::findByKind(const EphemerisEngineKind::Type kind) noexcept
{
    const auto& all = descriptors();
    for (const EphemerisEngineDescriptor& descriptor : all) {
        if (descriptor.kind == kind) {
            return &descriptor;
        }
    }
    return nullptr;
}

}  // namespace skygate::ephemeris
