#pragma once

#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"

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
        m_specifiedMask |= maskBit(Key::EngineKind);
    }

    [[nodiscard]] EphemerisEngineKind::Type engineKind() const
    {
        return m_engineKind;
    }

    void setCorrectionFlags(EphemerisCorrectionFlags correctionFlags)
    {
        m_correctionFlags = correctionFlags;
        m_specifiedMask |= maskBit(Key::CorrectionFlags);
    }

    [[nodiscard]] EphemerisCorrectionFlags correctionFlags() const
    {
        return m_correctionFlags;
    }

    void setFallbackToSimpleEngine(bool fallbackToSimpleEngine)
    {
        m_fallbackToSimpleEngine = fallbackToSimpleEngine;
        m_specifiedMask |= maskBit(Key::FallbackToSimpleEngine);
    }

    [[nodiscard]] bool fallbackToSimpleEngine() const
    {
        return m_fallbackToSimpleEngine;
    }

    void setEnableAtmosphericRefraction(bool enableAtmosphericRefraction)
    {
        m_enableAtmosphericRefraction = enableAtmosphericRefraction;
        m_specifiedMask |= maskBit(Key::EnableAtmosphericRefraction);
    }

    [[nodiscard]] bool enableAtmosphericRefraction() const
    {
        return m_enableAtmosphericRefraction;
    }

    void setAtmosphericPressureHpa(double atmosphericPressureHpa)
    {
        m_atmosphericPressureHpa = atmosphericPressureHpa;
        m_specifiedMask |= maskBit(Key::AtmosphericPressureHpa);
    }

    [[nodiscard]] double atmosphericPressureHpa() const
    {
        return m_atmosphericPressureHpa;
    }

    void setAtmosphericTemperatureC(double atmosphericTemperatureC)
    {
        m_atmosphericTemperatureC = atmosphericTemperatureC;
        m_specifiedMask |= maskBit(Key::AtmosphericTemperatureC);
    }

    [[nodiscard]] double atmosphericTemperatureC() const
    {
        return m_atmosphericTemperatureC;
    }

    void setRelativeHumidity(double relativeHumidity)
    {
        m_relativeHumidity = relativeHumidity;
        m_specifiedMask |= maskBit(Key::RelativeHumidity);
    }

    [[nodiscard]] double relativeHumidity() const
    {
        return m_relativeHumidity;
    }

    void setObservingWavelengthMicrometers(double observingWavelengthMicrometers)
    {
        m_observingWavelengthMicrometers = observingWavelengthMicrometers;
        m_specifiedMask |= maskBit(Key::ObservingWavelengthMicrometers);
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
    std::uint8_t m_specifiedMask = 0U;

    static constexpr std::uint8_t maskBit(const Key key) noexcept
    {
        return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(key));
    }
};

}  // namespace skygate::ephemeris
