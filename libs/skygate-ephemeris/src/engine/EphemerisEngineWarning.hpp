#pragma once

#include "TimeScaleConversionDiagnostics.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisEngineWarning {
public:
    enum class Code : std::uint8_t {
        AccuracyDegraded,
        UnsupportedBody,
        DataOutOfRange,
        MissingEphemerisData,
        MissingObserver,
        TimeScaleDataUnavailable,
        CorrectionUnavailable,
        ComputationFailed,
        BarycenterFallback,
        LeapSecondTableMissing,
        LeapSecondTableStale,
        EpochOutsideLeapSecondTable,
        LeapSecondFallbackApplied,
        UnsupportedTimeScaleConversion,
        InvalidExplicitEpoch,
        InvalidTimeScaleInput,
        TdbApproximationApplied,
        EarthOrientationDataMissing,
        EarthOrientationDataStale,
        EarthOrientationDataPredicted,
        EpochOutsideEarthOrientationData,
        DeltaTFallbackApplied,
        DeltaTUnavailable,
        EarthOrientationDataEstimated,
        EarthOrientationInvalidInput,
        // One past the last canonical code; keeps the inventory below in sync
        // with the enum through the static assertion.
        Last = EarthOrientationInvalidInput + 1
    };

    struct Detail {
        Code code = Code::AccuracyDegraded;
        std::string text;
    };

    constexpr EphemerisEngineWarning() noexcept = default;

    explicit constexpr EphemerisEngineWarning(const Code code) noexcept : m_code(code) {}

    [[nodiscard]] constexpr Code code() const noexcept
    {
        return m_code;
    }

    [[nodiscard]] constexpr std::string_view displayText() const noexcept
    {
        return text(m_code);
    }

    // Canonical inventory in declaration order. Consumers that render or
    // serialize the whole vocabulary iterate this list instead of restating
    // the codes, so a code added to the enum cannot be silently dropped by a
    // stale copy.
    [[nodiscard]] static constexpr std::span<const Code> allCodes() noexcept
    {
        return std::span<const Code>(kAllCodes);
    }

    [[nodiscard]] static constexpr std::string_view text(const Code code) noexcept
    {
        switch (code) {
        case Code::AccuracyDegraded:
            return "Result accuracy is degraded for this request.";
        case Code::UnsupportedBody:
            return "This body is not supported by the selected ephemeris engine.";
        case Code::DataOutOfRange:
            return "The request is outside the effective date range of the available ephemeris data.";
        case Code::MissingEphemerisData:
            return "Required ephemeris data is unavailable.";
        case Code::MissingObserver:
            return "Observer information is missing or invalid, so topocentric coordinates are unavailable.";
        case Code::TimeScaleDataUnavailable:
            return "Required time-scale data is unavailable.";
        case Code::CorrectionUnavailable:
            return "One or more requested correction terms could not be applied.";
        case Code::ComputationFailed:
            return "The ephemeris computation failed.";
        case Code::BarycenterFallback:
            return "A planetary-system barycenter was used because the requested body center is unavailable.";
        case Code::LeapSecondTableMissing:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::LeapSecondTableMissing
            );
        case Code::LeapSecondTableStale:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::LeapSecondTableStale
            );
        case Code::EpochOutsideLeapSecondTable:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable
            );
        case Code::LeapSecondFallbackApplied:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied
            );
        case Code::UnsupportedTimeScaleConversion:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::UnsupportedConversion
            );
        case Code::InvalidExplicitEpoch:
            return "The requested explicit epoch has non-finite Julian date parts.";
        case Code::InvalidTimeScaleInput:
            return TimeScaleConversionDiagnostics::warningText(TimeScaleConversionWarningCode::Type::InvalidInput);
        case Code::TdbApproximationApplied:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::TdbApproximationApplied
            );
        case Code::EarthOrientationDataMissing:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing
            );
        case Code::EarthOrientationDataStale:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::EarthOrientationDataStale
            );
        case Code::EarthOrientationDataPredicted:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted
            );
        case Code::EpochOutsideEarthOrientationData:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::EpochOutsideEarthOrientationData
            );
        case Code::DeltaTFallbackApplied:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied
            );
        case Code::DeltaTUnavailable:
            return TimeScaleConversionDiagnostics::warningText(TimeScaleConversionWarningCode::Type::DeltaTUnavailable);
        case Code::EarthOrientationDataEstimated:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::Type::EarthOrientationDataEstimated
            );
        case Code::EarthOrientationInvalidInput:
            return "Earth-orientation data input is invalid.";
        case Code::Last:
            break;
        }

        return "Ephemeris warning.";
    }

    // Time-scale warnings surface through the engine vocabulary. The text
    // delegation above already makes this header depend on the time-scale
    // enums, so the crosswalk cannot live on the source enum.
    [[nodiscard]] static constexpr Code fromTimeScaleWarning(const TimeScaleConversionWarningCode::Type code) noexcept
    {
        switch (code) {
        case TimeScaleConversionWarningCode::Type::LeapSecondTableMissing:
            return Code::LeapSecondTableMissing;
        case TimeScaleConversionWarningCode::Type::LeapSecondTableStale:
            return Code::LeapSecondTableStale;
        case TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable:
            return Code::EpochOutsideLeapSecondTable;
        case TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied:
            return Code::LeapSecondFallbackApplied;
        case TimeScaleConversionWarningCode::Type::UnsupportedConversion:
            return Code::UnsupportedTimeScaleConversion;
        case TimeScaleConversionWarningCode::Type::InvalidInput:
            return Code::InvalidTimeScaleInput;
        case TimeScaleConversionWarningCode::Type::TdbApproximationApplied:
            return Code::TdbApproximationApplied;
        case TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing:
            return Code::EarthOrientationDataMissing;
        case TimeScaleConversionWarningCode::Type::EarthOrientationDataStale:
            return Code::EarthOrientationDataStale;
        case TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted:
            return Code::EarthOrientationDataPredicted;
        case TimeScaleConversionWarningCode::Type::EpochOutsideEarthOrientationData:
            return Code::EpochOutsideEarthOrientationData;
        case TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied:
            return Code::DeltaTFallbackApplied;
        case TimeScaleConversionWarningCode::Type::DeltaTUnavailable:
            return Code::DeltaTUnavailable;
        case TimeScaleConversionWarningCode::Type::EarthOrientationDataEstimated:
            return Code::EarthOrientationDataEstimated;
        }

        return Code::AccuracyDegraded;
    }

private:
    inline static constexpr std::array<Code, 25U> kAllCodes{
        Code::AccuracyDegraded,
        Code::UnsupportedBody,
        Code::DataOutOfRange,
        Code::MissingEphemerisData,
        Code::MissingObserver,
        Code::TimeScaleDataUnavailable,
        Code::CorrectionUnavailable,
        Code::ComputationFailed,
        Code::BarycenterFallback,
        Code::LeapSecondTableMissing,
        Code::LeapSecondTableStale,
        Code::EpochOutsideLeapSecondTable,
        Code::LeapSecondFallbackApplied,
        Code::UnsupportedTimeScaleConversion,
        Code::InvalidExplicitEpoch,
        Code::InvalidTimeScaleInput,
        Code::TdbApproximationApplied,
        Code::EarthOrientationDataMissing,
        Code::EarthOrientationDataStale,
        Code::EarthOrientationDataPredicted,
        Code::EpochOutsideEarthOrientationData,
        Code::DeltaTFallbackApplied,
        Code::DeltaTUnavailable,
        Code::EarthOrientationDataEstimated,
        Code::EarthOrientationInvalidInput,
    };

    static_assert(
        kAllCodes.size() == static_cast<std::size_t>(Code::Last),
        "EphemerisEngineWarning::allCodes() must list every canonical code."
    );

    Code m_code = Code::AccuracyDegraded;
};

}  // namespace skygate::ephemeris
