#pragma once

#include "BitFlagSetOperations.hpp"
#include "EphemerisCorrectionFlags.hpp"
#include "EphemerisEngineKind.hpp"

#include <cmath>
#include <cstdint>

namespace skygate::ephemeris {

class EphemerisEngineOptions {
public:
    enum class Key : std::uint8_t {
        EngineKind,
        CorrectionFlags,
        FallbackToSimpleEngine,
        EnableAtmosphericRefraction,
        AtmosphericPressureHpa,
        AtmosphericTemperatureC,
        RelativeHumidity,
        ObservingWavelengthMicrometers
    };

    EphemerisEngineOptions() = default;

    void setEngineKind(EphemerisEngineKind::Type engineKind)
    {
        m_engineKind = engineKind;
        m_specifiedMask |= BitFlagSetOperations::bit(Key::EngineKind);
    }

    [[nodiscard]] EphemerisEngineKind::Type engineKind() const
    {
        return m_engineKind;
    }

    void setCorrectionFlags(EphemerisCorrectionFlags correctionFlags)
    {
        m_correctionFlags = correctionFlags;
        m_specifiedMask |= BitFlagSetOperations::bit(Key::CorrectionFlags);
    }

    [[nodiscard]] EphemerisCorrectionFlags correctionFlags() const
    {
        return m_correctionFlags;
    }

    void setFallbackToSimpleEngine(bool fallbackToSimpleEngine)
    {
        m_fallbackToSimpleEngine = fallbackToSimpleEngine;
        m_specifiedMask |= BitFlagSetOperations::bit(Key::FallbackToSimpleEngine);
    }

    [[nodiscard]] bool fallbackToSimpleEngine() const
    {
        return m_fallbackToSimpleEngine;
    }

    void setEnableAtmosphericRefraction(bool enableAtmosphericRefraction)
    {
        m_enableAtmosphericRefraction = enableAtmosphericRefraction;
        m_specifiedMask |= BitFlagSetOperations::bit(Key::EnableAtmosphericRefraction);
    }

    [[nodiscard]] bool enableAtmosphericRefraction() const
    {
        return m_enableAtmosphericRefraction;
    }

    [[nodiscard]] bool requestsAtmosphericRefraction() const noexcept
    {
        return m_enableAtmosphericRefraction
               && EphemerisCorrectionFlags::has(m_correctionFlags, EphemerisCorrectionFlags::atmosphericRefraction());
    }

    void setAtmosphericPressureHpa(double atmosphericPressureHpa)
    {
        if (!std::isfinite(atmosphericPressureHpa)) {
            return;
        }
        m_atmosphericPressureHpa = atmosphericPressureHpa;
        m_specifiedMask |= BitFlagSetOperations::bit(Key::AtmosphericPressureHpa);
    }

    [[nodiscard]] double atmosphericPressureHpa() const
    {
        return m_atmosphericPressureHpa;
    }

    void setAtmosphericTemperatureC(double atmosphericTemperatureC)
    {
        m_atmosphericTemperatureC = atmosphericTemperatureC;
        m_specifiedMask |= BitFlagSetOperations::bit(Key::AtmosphericTemperatureC);
    }

    [[nodiscard]] double atmosphericTemperatureC() const
    {
        return m_atmosphericTemperatureC;
    }

    void setRelativeHumidity(double relativeHumidity)
    {
        if (!std::isfinite(relativeHumidity)) {
            return;
        }
        m_relativeHumidity = relativeHumidity;
        m_specifiedMask |= BitFlagSetOperations::bit(Key::RelativeHumidity);
    }

    [[nodiscard]] double relativeHumidity() const
    {
        return m_relativeHumidity;
    }

    void setObservingWavelengthMicrometers(double observingWavelengthMicrometers)
    {
        m_observingWavelengthMicrometers = observingWavelengthMicrometers;
        m_specifiedMask |= BitFlagSetOperations::bit(Key::ObservingWavelengthMicrometers);
    }

    [[nodiscard]] double observingWavelengthMicrometers() const
    {
        return m_observingWavelengthMicrometers;
    }

    [[nodiscard]] bool isOptionSet(Key key) const noexcept;
    [[nodiscard]] bool operator==(const EphemerisEngineOptions& other) const noexcept;

private:
    EphemerisEngineKind::Type m_engineKind = EphemerisEngineKind::Type::Simple;
    EphemerisCorrectionFlags m_correctionFlags = EphemerisCorrectionFlags::apparentTopocentric();
    bool m_fallbackToSimpleEngine = true;
    bool m_enableAtmosphericRefraction = true;
    double m_atmosphericPressureHpa = 1013.25;
    double m_atmosphericTemperatureC = 10.0;
    double m_relativeHumidity = 0.0;
    double m_observingWavelengthMicrometers = 0.55;
    std::uint32_t m_specifiedMask = 0U;
};

}  // namespace skygate::ephemeris
