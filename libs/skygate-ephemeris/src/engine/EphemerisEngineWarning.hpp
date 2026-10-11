#pragma once

#include "TimeScaleConversionDiagnostics.hpp"

#include <cstdint>
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
        EarthOrientationStaleData,
        EarthOrientationPredictedData,
        EarthOrientationMissingData,
        EarthOrientationEpochOutsideRange,
        EarthOrientationInvalidInput,
        EarthOrientationEstimatedData
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
            return TimeScaleConversionDiagnostics::warningText(TimeScaleConversionWarningCode::LeapSecondTableMissing);
        case Code::LeapSecondTableStale:
            return TimeScaleConversionDiagnostics::warningText(TimeScaleConversionWarningCode::LeapSecondTableStale);
        case Code::EpochOutsideLeapSecondTable:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable
            );
        case Code::LeapSecondFallbackApplied:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::LeapSecondFallbackApplied
            );
        case Code::UnsupportedTimeScaleConversion:
            return TimeScaleConversionDiagnostics::warningText(TimeScaleConversionWarningCode::UnsupportedConversion);
        case Code::InvalidExplicitEpoch:
            return "The requested explicit epoch has non-finite Julian date parts.";
        case Code::InvalidTimeScaleInput:
            return TimeScaleConversionDiagnostics::warningText(TimeScaleConversionWarningCode::InvalidInput);
        case Code::TdbApproximationApplied:
            return TimeScaleConversionDiagnostics::warningText(TimeScaleConversionWarningCode::TdbApproximationApplied);
        case Code::EarthOrientationDataMissing:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EarthOrientationDataMissing
            );
        case Code::EarthOrientationDataStale:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EarthOrientationDataStale
            );
        case Code::EarthOrientationDataPredicted:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EarthOrientationDataPredicted
            );
        case Code::EpochOutsideEarthOrientationData:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData
            );
        case Code::DeltaTFallbackApplied:
            return TimeScaleConversionDiagnostics::warningText(TimeScaleConversionWarningCode::DeltaTFallbackApplied);
        case Code::DeltaTUnavailable:
            return TimeScaleConversionDiagnostics::warningText(TimeScaleConversionWarningCode::DeltaTUnavailable);
        case Code::EarthOrientationDataEstimated:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EarthOrientationDataEstimated
            );
        case Code::EarthOrientationStaleData:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EarthOrientationDataStale
            );
        case Code::EarthOrientationPredictedData:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EarthOrientationDataPredicted
            );
        case Code::EarthOrientationMissingData:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EarthOrientationDataMissing
            );
        case Code::EarthOrientationEpochOutsideRange:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData
            );
        case Code::EarthOrientationInvalidInput:
            return "Earth-orientation data input is invalid.";
        case Code::EarthOrientationEstimatedData:
            return TimeScaleConversionDiagnostics::warningText(
                TimeScaleConversionWarningCode::EarthOrientationDataEstimated
            );
        }

        return "Ephemeris warning.";
    }

private:
    Code m_code = Code::AccuracyDegraded;
};

}  // namespace skygate::ephemeris
