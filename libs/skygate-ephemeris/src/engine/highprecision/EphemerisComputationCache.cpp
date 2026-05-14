#include "engine/highprecision/EphemerisComputationCache.hpp"

#include <cstdint>
#include <iomanip>
#include <ios>
#include <sstream>

namespace skygate::ephemeris::highprecision {
namespace {

void appendDouble(std::ostringstream& stream, const double value)
{
    stream << std::hexfloat << value << std::defaultfloat;
}

void appendEpoch(std::ostringstream& stream, const AstronomicalEpoch& epoch)
{
    appendDouble(stream, epoch.julianDatePart1);
    stream << ',';
    appendDouble(stream, epoch.julianDatePart2);
    stream << ',' << static_cast<int>(epoch.timeScale);
}

void appendOptions(std::ostringstream& stream, const EphemerisEngineOptions& options)
{
    stream << static_cast<int>(options.engineKind) << ',' << static_cast<std::uint32_t>(options.correctionFlags) << ','
           << options.fallbackToSimpleEngine << ',' << options.enableAtmosphericRefraction << ',';
    appendDouble(stream, options.atmosphericPressureHpa);
    stream << ',';
    appendDouble(stream, options.atmosphericTemperatureC);
    stream << ',';
    appendDouble(stream, options.relativeHumidity);
    stream << ',';
    appendDouble(stream, options.observingWavelengthMicrometers);
}

void appendObserver(std::ostringstream& stream, const core::GeoLocation& observer)
{
    appendDouble(stream, observer.latitudeDeg);
    stream << ',';
    appendDouble(stream, observer.longitudeDeg);
    stream << ',';
    appendDouble(stream, observer.elevationMeters);
}

}  // namespace

EphemerisComputationCache::EphemerisComputationCache(const std::size_t maxSnapshotEntries)
    : m_maxSnapshotEntries(maxSnapshotEntries == 0U ? 1U : maxSnapshotEntries)
{
}

std::optional<SkySnapshot> EphemerisComputationCache::findSnapshot(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo
) const
{
    const std::string key = makeSnapshotKey(request, catalogBodies, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    const auto snapshot = m_snapshots.find(key);
    if (snapshot == m_snapshots.end()) {
        return std::nullopt;
    }

    return snapshot->second;
}

void EphemerisComputationCache::storeSnapshot(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo,
    const SkySnapshot& snapshot
) const
{
    const std::string key = makeSnapshotKey(request, catalogBodies, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    if (!m_snapshots.contains(key)) {
        m_snapshotOrder.push_back(key);
    }
    m_snapshots[key] = snapshot;

    while (m_snapshotOrder.size() > m_maxSnapshotEntries) {
        m_snapshots.erase(m_snapshotOrder.front());
        m_snapshotOrder.pop_front();
    }
}

void EphemerisComputationCache::clear() const
{
    const std::scoped_lock lock(m_mutex);
    m_snapshotOrder.clear();
    m_snapshots.clear();
}

std::string EphemerisComputationCache::makeSnapshotKey(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo
)
{
    std::ostringstream stream;
    stream << "epoch:";
    appendEpoch(stream, request.epoch);
    stream << "|utc:" << request.context.utcTime.time_since_epoch().count();
    stream << "|observer:";
    appendObserver(stream, request.context.observer);
    stream << "|options:";
    appendOptions(stream, request.options);
    stream << "|catalog:" << static_cast<const void*>(catalogBodies.data()) << ',' << catalogBodies.size();
    stream << "|dataset:" << dataSetInfo.id << ',' << dataSetInfo.version << ',' << dataSetInfo.provenance << ','
           << dataSetInfo.dateRanges.size();
    return stream.str();
}

}  // namespace skygate::ephemeris::highprecision
