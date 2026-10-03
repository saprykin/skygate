#pragma once

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
            return "Leap-second table data is unavailable.";
        case Code::LeapSecondTableStale:
            return "Leap-second table data is stale for this conversion.";
        case Code::EpochOutsideLeapSecondTable:
            return "The requested epoch is outside the leap-second table validity range.";
        case Code::LeapSecondFallbackApplied:
            return "A degraded leap-second fallback offset was applied.";
        case Code::UnsupportedTimeScaleConversion:
            return "The requested time-scale conversion is not supported.";
        case Code::InvalidExplicitEpoch:
            return "The requested explicit epoch has non-finite Julian date parts.";
        case Code::InvalidTimeScaleInput:
            return "The requested time-scale conversion input is invalid.";
        case Code::TdbApproximationApplied:
            return "The TT/TDB conversion used a documented approximation.";
        case Code::EarthOrientationDataMissing:
            return "Earth-orientation data is unavailable.";
        case Code::EarthOrientationDataStale:
            return "Earth-orientation data is stale for this conversion.";
        case Code::EarthOrientationDataPredicted:
            return "Earth-orientation data uses a prediction for this conversion.";
        case Code::EpochOutsideEarthOrientationData:
            return "The requested epoch is outside the Earth-orientation data range.";
        case Code::DeltaTFallbackApplied:
            return "A degraded Delta T fallback was applied.";
        case Code::DeltaTUnavailable:
            return "Delta T data is unavailable for this conversion.";
        case Code::EarthOrientationDataEstimated:
            return "Earth-orientation data uses an estimate for this conversion.";
        case Code::EarthOrientationStaleData:
            return "Earth-orientation data is stale for this conversion.";
        case Code::EarthOrientationPredictedData:
            return "Earth-orientation data uses a prediction for this conversion.";
        case Code::EarthOrientationMissingData:
            return "Earth-orientation data is unavailable.";
        case Code::EarthOrientationEpochOutsideRange:
            return "The requested epoch is outside the Earth-orientation data range.";
        case Code::EarthOrientationInvalidInput:
            return "Earth-orientation data input is invalid.";
        case Code::EarthOrientationEstimatedData:
            return "Earth-orientation data uses an estimate for this conversion.";
        }

        return "Ephemeris warning.";
    }

    [[nodiscard]] static constexpr std::uint32_t mask(const Code code) noexcept
    {
        return 1U << static_cast<std::uint8_t>(code);
    }

private:
    Code m_code = Code::AccuracyDegraded;
};

}  // namespace skygate::ephemeris
