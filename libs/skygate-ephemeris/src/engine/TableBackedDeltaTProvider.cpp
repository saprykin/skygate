#include "TableBackedDeltaTProvider.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace skygate::ephemeris {

namespace {

[[nodiscard]] bool
epochWithinRange(const skygate::core::AstronomicalEpoch& epoch, const EphemerisDateRange& range) noexcept
{
    if (!epoch.isFinite()) {
        return false;
    }

    const double key = epoch.sortKey();
    return key >= range.start.sortKey() && key <= range.end.sortKey();
}

[[nodiscard]] IDeltaTProvider::Estimate unavailableEstimate(std::string diagnosticText)
{
    IDeltaTProvider::Estimate estimate;
    estimate.status = IDeltaTProvider::EstimateStatus::Unavailable;
    estimate.diagnosticText = std::move(diagnosticText);
    return estimate;
}

}  // namespace

TableBackedDeltaTProvider::TableBackedDeltaTProvider(DataInfo dataInfo, std::vector<TableEntry> entries)
    : m_dataInfo(std::move(dataInfo)), m_entries(std::move(entries))
{
}

const IDeltaTProvider::DataInfo& TableBackedDeltaTProvider::dataInfo() const noexcept
{
    return m_dataInfo;
}

std::span<const IDeltaTProvider::TableEntry> TableBackedDeltaTProvider::entries() const noexcept
{
    return m_entries;
}

IDeltaTProvider::Estimate TableBackedDeltaTProvider::deltaTSeconds(const skygate::core::AstronomicalEpoch& epoch) const
{
    if (!epoch.isFinite()) {
        return unavailableEstimate("Delta T estimate requires a finite epoch.");
    }

    if (m_entries.empty()) {
        return unavailableEstimate("Delta T data contains no table entries.");
    }

    const double requestedEpochKey = epoch.sortKey();
    if (requestedEpochKey >= m_entries.front().effectiveUtcEpoch.sortKey()
        && requestedEpochKey <= m_entries.back().effectiveUtcEpoch.sortKey()) {
        const auto upper = std::ranges::lower_bound(m_entries, requestedEpochKey, {}, [](const TableEntry& entry) {
            return entry.effectiveUtcEpoch.sortKey();
        });
        if (upper == m_entries.begin()) {
            return Estimate{
                .status = EstimateStatus::Available,
                .deltaTSeconds = upper->deltaTSeconds,
                .diagnosticText = "Delta T estimate loaded from table.",
                .provenance = m_dataInfo.provenance,
            };
        }
        if (upper == m_entries.end()) {
            return Estimate{
                .status = EstimateStatus::Available,
                .deltaTSeconds = m_entries.back().deltaTSeconds,
                .diagnosticText = "Delta T estimate loaded from table.",
                .provenance = m_dataInfo.provenance,
            };
        }

        const TableEntry& before = *(upper - 1);
        const TableEntry& after = *upper;
        const double beforeKey = before.effectiveUtcEpoch.sortKey();
        const double afterKey = after.effectiveUtcEpoch.sortKey();
        if (!(afterKey > beforeKey)) {
            return unavailableEstimate("Delta T data contains duplicate or non-monotonic effective UTC epochs.");
        }

        const double ratio = (requestedEpochKey - beforeKey) / (afterKey - beforeKey);
        return Estimate{
            .status = EstimateStatus::Available,
            .deltaTSeconds = before.deltaTSeconds + (after.deltaTSeconds - before.deltaTSeconds) * ratio,
            .diagnosticText = "Delta T estimate interpolated from table.",
            .provenance = m_dataInfo.provenance,
        };
    }

    if (m_dataInfo.ancientFallbackModel.has_value()
        && epochWithinRange(epoch, m_dataInfo.ancientFallbackModel->validityRange)) {
        const DataInfo::FallbackModelInfo& fallback = *m_dataInfo.ancientFallbackModel;
        return Estimate{
            .status = EstimateStatus::Degraded,
            .deltaTSeconds = fallback.representativeDeltaTSeconds,
            .estimatedUncertaintySeconds = fallback.estimatedUncertaintySeconds,
            .diagnosticText = "Delta T estimate uses ancient fallback model metadata.",
            .provenance = fallback.provenance,
        };
    }

    return unavailableEstimate("Requested epoch is outside Delta T data coverage.");
}

}  // namespace skygate::ephemeris
