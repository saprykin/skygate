#pragma once

#include "BitFlagSet.hpp"
#include "EphemerisCorrectionFlags.hpp"
#include "EphemerisDataSourceProvenance.hpp"
#include "EphemerisEngineQueryStatus.hpp"
#include "EphemerisEngineWarning.hpp"
#include "time/EphemerisDateRange.hpp"

#include <bit>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

struct EphemerisEngineQueryResult {
    using WarningMask = BitFlagSet<EphemerisEngineWarning::Code>;

    EphemerisEngineQueryStatus::Type status = EphemerisEngineQueryStatus::Type::Valid;
    WarningMask warningCodeMask;
    std::vector<EphemerisEngineWarning::Detail> warningDetails;
    std::string dataSourceProvenance;
    EphemerisDataSourceProvenance::Type dataSourceProvenanceKind = EphemerisDataSourceProvenance::Type::Unknown;
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
        warningCodeMask.add(WarningMask::bit(code));
    }

    void addWarning(const EphemerisEngineWarning::Code code, std::string text)
    {
        addWarning(code);
        warningDetails.push_back(EphemerisEngineWarning::Detail{code, std::move(text)});
    }

    void addUnavailableCorrection(const EphemerisCorrectionFlags correction) noexcept
    {
        if (correction == EphemerisCorrectionFlags::noCorrections()) {
            return;
        }
        unavailableCorrections |= correction;
        addWarning(EphemerisEngineWarning::Code::CorrectionUnavailable);
        if (status == EphemerisEngineQueryStatus::Type::Valid) {
            status = EphemerisEngineQueryStatus::Type::Degraded;
        }
    }

    void finalizeCorrectionTracking(const EphemerisCorrectionFlags requested) noexcept
    {
        requestedCorrections = requested;
        const EphemerisCorrectionFlags accountedCorrections = appliedCorrections | unavailableCorrections;
        skippedCorrections = EphemerisCorrectionFlags::without(requestedCorrections, accountedCorrections);
    }

    [[nodiscard]] bool hasWarning(const EphemerisEngineWarning::Code code) const noexcept
    {
        return warningCodeMask.has(WarningMask::bit(code));
    }

    [[nodiscard]] std::size_t warningCount() const noexcept
    {
        return static_cast<std::size_t>(std::popcount(warningCodeMask.bits()));
    }

    [[nodiscard]] bool hasWarnings() const noexcept
    {
        return warningCodeMask.hasAny();
    }
};

}  // namespace skygate::ephemeris
