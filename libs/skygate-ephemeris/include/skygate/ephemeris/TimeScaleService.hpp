#pragma once

#include "skygate/ephemeris/LeapSecondProvider.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

enum class TimeScaleConversionStatus : std::uint8_t {
    Valid,
    Degraded,
    Failed
};

[[nodiscard]] constexpr std::string_view displayName(const TimeScaleConversionStatus status) noexcept
{
    switch (status) {
    case TimeScaleConversionStatus::Valid:
        return "valid";
    case TimeScaleConversionStatus::Degraded:
        return "degraded";
    case TimeScaleConversionStatus::Failed:
        return "failed";
    }

    return {};
}

enum class TimeScaleConversionWarningCode : std::uint8_t {
    LeapSecondTableMissing,
    LeapSecondTableStale,
    EpochOutsideLeapSecondTable,
    LeapSecondFallbackApplied,
    UnsupportedConversion,
    InvalidInput
};

[[nodiscard]] constexpr std::string_view timeScaleConversionWarningText(const TimeScaleConversionWarningCode code
) noexcept
{
    switch (code) {
    case TimeScaleConversionWarningCode::LeapSecondTableMissing:
        return "Leap-second table data is unavailable.";
    case TimeScaleConversionWarningCode::LeapSecondTableStale:
        return "Leap-second table data is stale for this conversion.";
    case TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable:
        return "The requested epoch is outside the leap-second table validity range.";
    case TimeScaleConversionWarningCode::LeapSecondFallbackApplied:
        return "A degraded leap-second fallback offset was applied.";
    case TimeScaleConversionWarningCode::UnsupportedConversion:
        return "The requested time-scale conversion is not supported.";
    case TimeScaleConversionWarningCode::InvalidInput:
        return "The requested time-scale conversion input is invalid.";
    }

    return "Time-scale conversion warning.";
}

[[nodiscard]] constexpr std::uint32_t timeScaleConversionWarningMask(const TimeScaleConversionWarningCode code) noexcept
{
    return 1U << static_cast<std::uint8_t>(code);
}

struct TimeScaleConversionResult {
    AstronomicalEpoch epoch;
    TimeScaleConversionStatus status = TimeScaleConversionStatus::Failed;
    std::uint32_t warningCodeMask = 0U;
    std::string diagnosticText;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == TimeScaleConversionStatus::Valid || status == TimeScaleConversionStatus::Degraded;
    }

    void addWarning(const TimeScaleConversionWarningCode code) noexcept
    {
        warningCodeMask |= timeScaleConversionWarningMask(code);
    }

    [[nodiscard]] bool hasWarning(const TimeScaleConversionWarningCode code) const noexcept
    {
        return (warningCodeMask & timeScaleConversionWarningMask(code)) != 0U;
    }
};

struct TimeScaleServiceOptions {
    bool allowDegradedLeapSecondFallback = false;
    int fallbackTaiMinusUtcSeconds = 0;
};

class ITimeScaleService {
public:
    virtual ~ITimeScaleService() = default;

    [[nodiscard]] virtual TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, TimeScale targetScale) const = 0;

    [[nodiscard]] virtual TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, TimeScale targetScale) const = 0;
};

class LeapSecondTimeScaleService final : public ITimeScaleService {
public:
    explicit LeapSecondTimeScaleService(
        std::shared_ptr<const ILeapSecondProvider> leapSecondProvider, TimeScaleServiceOptions options = {}
    );

    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, TimeScale targetScale) const override;

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, TimeScale targetScale) const override;

private:
    std::shared_ptr<const ILeapSecondProvider> m_leapSecondProvider;
    TimeScaleServiceOptions m_options;
};

}  // namespace skygate::ephemeris
