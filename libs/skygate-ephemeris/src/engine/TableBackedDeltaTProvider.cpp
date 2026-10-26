#include "TableBackedDeltaTProvider.hpp"
#include "TableIntervalLookup.hpp"
#include "TableInterpolation.hpp"

#include <span>
#include <string>
#include <utility>

namespace skygate::ephemeris {

namespace {

[[nodiscard]] IDeltaTProvider::Estimate unavailableEstimate(std::string diagnosticText)
{
    IDeltaTProvider::Estimate estimate;
    estimate.status = IDeltaTProvider::EstimateStatus::Unavailable;
    estimate.diagnosticText = std::move(diagnosticText);
    return estimate;
}

[[nodiscard]] IDeltaTProvider::Estimate
tableEstimate(const double deltaTSeconds, std::string diagnosticText, const std::string& provenance)
{
    return IDeltaTProvider::Estimate{
        .status = IDeltaTProvider::EstimateStatus::Available,
        .deltaTSeconds = deltaTSeconds,
        .diagnosticText = std::move(diagnosticText),
        .provenance = provenance,
    };
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
        const TableIntervalLookup::Result interval = TableIntervalLookup::locate(
            std::span<const TableEntry>(m_entries), requestedEpochKey, [](const TableEntry& entry) noexcept {
                return entry.effectiveUtcEpoch.sortKey();
            }
        );
        if (!interval.hasLowerRow()) {
            return tableEstimate(
                m_entries.front().deltaTSeconds, "Delta T estimate loaded from table.", m_dataInfo.provenance
            );
        }
        if (!interval.hasUpperRow()) {
            return tableEstimate(
                m_entries.back().deltaTSeconds, "Delta T estimate loaded from table.", m_dataInfo.provenance
            );
        }

        const TableEntry& before = m_entries[interval.upperIndex - 1U];
        const TableEntry& after = m_entries[interval.upperIndex];
        const std::optional<double> ratio = TableInterpolation::ratio(
            requestedEpochKey, before.effectiveUtcEpoch.sortKey(), after.effectiveUtcEpoch.sortKey()
        );
        if (!ratio.has_value()) {
            return unavailableEstimate("Delta T data contains duplicate or non-monotonic effective UTC epochs.");
        }

        return tableEstimate(
            TableInterpolation::lerp(before.deltaTSeconds, after.deltaTSeconds, *ratio),
            "Delta T estimate interpolated from table.",
            m_dataInfo.provenance
        );
    }

    if (m_dataInfo.ancientFallbackModel.has_value() && m_dataInfo.ancientFallbackModel->validityRange.contains(epoch)) {
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
