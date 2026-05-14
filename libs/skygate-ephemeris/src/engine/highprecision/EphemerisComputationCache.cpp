#include "engine/highprecision/EphemerisComputationCache.hpp"

#include <cstdint>
#include <iomanip>
#include <ios>
#include <optional>
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

void appendDateRange(std::ostringstream& stream, const EphemerisDateRange& range)
{
    stream << range.id << ',' << range.displayName << ',';
    appendEpoch(stream, range.start);
    stream << ',';
    appendEpoch(stream, range.end);
}

void appendOptionalDateRange(std::ostringstream& stream, const std::optional<EphemerisDateRange>& range)
{
    stream << range.has_value();
    if (!range.has_value()) {
        return;
    }
    stream << ',';
    appendDateRange(stream, *range);
}

void appendOptionalDouble(std::ostringstream& stream, const std::optional<double>& value)
{
    stream << value.has_value();
    if (!value.has_value()) {
        return;
    }
    stream << ',';
    appendDouble(stream, *value);
}

void appendOptionalEquatorial(std::ostringstream& stream, const std::optional<core::EquatorialCoordinate>& coordinate)
{
    stream << coordinate.has_value();
    if (!coordinate.has_value()) {
        return;
    }
    stream << ',';
    appendDouble(stream, coordinate->rightAscensionHours);
    stream << ',';
    appendDouble(stream, coordinate->declinationDeg);
}

void appendOptionalAstrometry(std::ostringstream& stream, const std::optional<CatalogStarAstrometry>& astrometry)
{
    stream << astrometry.has_value();
    if (!astrometry.has_value()) {
        return;
    }
    stream << ',';
    appendDouble(stream, astrometry->referenceEquatorial.rightAscensionHours);
    stream << ',';
    appendDouble(stream, astrometry->referenceEquatorial.declinationDeg);
    stream << ',';
    appendEpoch(stream, astrometry->referenceEpoch);
    stream << ',';
    appendOptionalDouble(stream, astrometry->properMotionRightAscensionMasPerYear);
    stream << ',';
    appendOptionalDouble(stream, astrometry->properMotionDeclinationMasPerYear);
    stream << ',';
    appendOptionalDouble(stream, astrometry->stellarParallaxMas);
    stream << ',';
    appendOptionalDouble(stream, astrometry->radialVelocityKmPerSecond);
    stream << ',';
    appendOptionalDateRange(stream, astrometry->validityRange);
}

void appendDeepSkyObject(std::ostringstream& stream, const std::optional<DeepSkyObjectInfo>& deepSkyObject)
{
    stream << deepSkyObject.has_value();
    if (!deepSkyObject.has_value()) {
        return;
    }
    stream << ',' << static_cast<int>(deepSkyObject->kind) << ',';
    appendOptionalDouble(stream, deepSkyObject->majorAxisArcmin);
    stream << ',';
    appendOptionalDouble(stream, deepSkyObject->minorAxisArcmin);
    stream << ',';
    appendOptionalDouble(stream, deepSkyObject->positionAngleDeg);
    stream << ",aliases:" << deepSkyObject->aliases.size();
    for (const std::string& alias : deepSkyObject->aliases) {
        stream << ',' << alias;
    }
}

void appendCatalogBody(std::ostringstream& stream, const CelestialBody& body)
{
    stream << body.id << ',' << body.displayName << ',' << static_cast<int>(body.type) << ','
           << static_cast<int>(body.ephemerisSource) << ',';
    appendDouble(stream, body.visualMagnitude);
    stream << ',';
    appendOptionalEquatorial(stream, body.fixedEquatorial);
    stream << ',';
    appendOptionalAstrometry(stream, body.starAstrometry);
    stream << ',';
    appendDeepSkyObject(stream, body.deepSkyObject);
}

}  // namespace

EphemerisComputationCache::EphemerisComputationCache(const std::size_t maxEntries)
    : m_maxEntries(maxEntries == 0U ? 1U : maxEntries)
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

    while (m_snapshotOrder.size() > m_maxEntries) {
        m_snapshots.erase(m_snapshotOrder.front());
        m_snapshotOrder.pop_front();
    }
}

std::shared_ptr<const PreparedEphemerisRequestState> EphemerisComputationCache::findPreparedRequestState(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo
) const
{
    const std::string key = makePreparedStateKey(request, catalogBodies, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    const auto preparedState = m_preparedStates.find(key);
    if (preparedState == m_preparedStates.end()) {
        return nullptr;
    }

    return preparedState->second;
}

void EphemerisComputationCache::storePreparedRequestState(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedState
) const
{
    if (preparedState == nullptr) {
        return;
    }

    const std::string key = makePreparedStateKey(request, catalogBodies, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    if (!m_preparedStates.contains(key)) {
        m_preparedStateOrder.push_back(key);
    }
    m_preparedStates[key] = std::move(preparedState);

    while (m_preparedStateOrder.size() > m_maxEntries) {
        m_preparedStates.erase(m_preparedStateOrder.front());
        m_preparedStateOrder.pop_front();
    }
}

void EphemerisComputationCache::clear() const
{
    const std::scoped_lock lock(m_mutex);
    m_snapshotOrder.clear();
    m_snapshots.clear();
    m_preparedStateOrder.clear();
    m_preparedStates.clear();
}

std::string EphemerisComputationCache::makeRequestKey(
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
    stream << "|catalog:" << catalogBodies.size();
    for (const CelestialBody& body : catalogBodies) {
        stream << ';';
        appendCatalogBody(stream, body);
    }
    stream << "|dataset:" << dataSetInfo.id << ',' << dataSetInfo.displayName << ',' << dataSetInfo.version << ','
           << dataSetInfo.provenance << ',' << dataSetInfo.dateRanges.size();
    for (const EphemerisDateRange& range : dataSetInfo.dateRanges) {
        stream << ';';
        appendDateRange(stream, range);
    }
    return stream.str();
}

std::string EphemerisComputationCache::makeSnapshotKey(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo
)
{
    return "snapshot|" + makeRequestKey(request, catalogBodies, dataSetInfo);
}

std::string EphemerisComputationCache::makePreparedStateKey(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo
)
{
    return "prepared|" + makeRequestKey(request, catalogBodies, dataSetInfo);
}

}  // namespace skygate::ephemeris::highprecision
