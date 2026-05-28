#pragma once

#include "EphemerisCorrectionFlags.hpp"
#include "EphemerisDateRange.hpp"
#include "EphemerisEngineQueryStatus.hpp"
#include "EphemerisEngineWarning.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace skygate::ephemeris {

struct EphemerisEngineQueryResult {
    EphemerisEngineQueryStatus::Type status = EphemerisEngineQueryStatus::Type::Valid;
    std::uint32_t warningCodeMask = 0U;
    std::string dataSourceProvenance;
    std::optional<EphemerisDateRange> effectiveDataValidityRange;
    std::optional<double> estimatedAngularUncertaintyArcsec;
    EphemerisCorrectionFlags requestedCorrections = EphemerisCorrectionFlags::noCorrections();
    EphemerisCorrectionFlags appliedCorrections = EphemerisCorrectionFlags::noCorrections();
    EphemerisCorrectionFlags skippedCorrections = EphemerisCorrectionFlags::noCorrections();
    EphemerisCorrectionFlags unavailableCorrections = EphemerisCorrectionFlags::noCorrections();

    [[nodiscard]] bool isSuccessful() const noexcept
    {
        return status == EphemerisEngineQueryStatus::Type::Valid
               || status == EphemerisEngineQueryStatus::Type::Degraded;
    }

    void addWarning(const EphemerisEngineWarning::Code code) noexcept
    {
        warningCodeMask |= EphemerisEngineWarning::mask(code);
    }

    void addUnavailableCorrection(const EphemerisCorrectionFlags correction) noexcept
    {
        unavailableCorrections |= correction;
        addWarning(EphemerisEngineWarning::Code::CorrectionUnavailable);
    }

    void finalizeCorrectionTracking(const EphemerisCorrectionFlags requested) noexcept
    {
        requestedCorrections = requested;
        const EphemerisCorrectionFlags accountedCorrections = appliedCorrections | unavailableCorrections;
        skippedCorrections = EphemerisCorrectionFlags::without(requestedCorrections, accountedCorrections);
    }

    [[nodiscard]] bool hasWarning(const EphemerisEngineWarning::Code code) const noexcept
    {
        return (warningCodeMask & EphemerisEngineWarning::mask(code)) != 0U;
    }

    [[nodiscard]] std::size_t warningCount() const noexcept
    {
        return static_cast<std::size_t>(std::popcount(warningCodeMask));
    }

    [[nodiscard]] bool hasWarnings() const noexcept
    {
        return warningCodeMask != 0U;
    }
};

}  // namespace skygate::ephemeris
