#pragma once

#include "IDeltaTProvider.hpp"
#include "IEarthOrientationProvider.hpp"
#include "ILeapSecondProvider.hpp"
#include "LeapSecondOffsetResolver.hpp"
#include "TimeScaleConversionResult.hpp"
#include "TimeScaleServiceOptions.hpp"

#include <memory>
#include <optional>
#include <string>

namespace skygate::ephemeris {

class Ut1OffsetResolver final {
public:
    struct Result {
        std::optional<double> ut1MinusUtcSeconds;
        TimeScaleConversionStatus::Type status = TimeScaleConversionStatus::Type::Valid;
        TimeScaleConversionResult::WarningMask warningCodeMask;
        std::string diagnosticText;

        void addWarning(const TimeScaleConversionWarningCode::Type code) noexcept;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return status == TimeScaleConversionStatus::Type::Valid
                   || status == TimeScaleConversionStatus::Type::Degraded;
        }
    };

    Ut1OffsetResolver(
        std::shared_ptr<const ILeapSecondProvider> leapSecondProvider,
        std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider,
        std::shared_ptr<const IDeltaTProvider> deltaTProvider,
        TimeScaleServiceOptions options
    );

    [[nodiscard]] Result lookupUtcToUt1Offset(const skygate::core::AstronomicalEpoch& utcEpoch) const;
    [[nodiscard]] TimeScaleConversionResult lookupUtcFromUt1(const skygate::core::AstronomicalEpoch& ut1Epoch) const;

    static void mergeWarnings(TimeScaleConversionResult& result, const Result& lookup) noexcept;

private:
    std::shared_ptr<const IEarthOrientationProvider> m_earthOrientationProvider;
    std::shared_ptr<const IDeltaTProvider> m_deltaTProvider;
    LeapSecondOffsetResolver m_leapSecondOffsetResolver;
    TimeScaleServiceOptions m_options;
};

}  // namespace skygate::ephemeris
