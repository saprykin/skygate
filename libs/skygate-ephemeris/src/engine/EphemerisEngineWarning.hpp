#pragma once

#include <cstdint>
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
        BarycenterFallback
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
