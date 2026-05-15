#include "engine/highprecision/EphemerisComputationCache.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ULL;

void mixByte(std::uint64_t& hash, const std::uint8_t value) noexcept
{
    hash ^= value;
    hash *= kFnvPrime;
}

void mixUint64(std::uint64_t& hash, std::uint64_t value) noexcept
{
    for (int byteIndex = 0; byteIndex < 8; ++byteIndex) {
        mixByte(hash, static_cast<std::uint8_t>(value & 0xffU));
        value >>= 8U;
    }
}

void mixBool(std::uint64_t& hash, const bool value) noexcept
{
    mixByte(hash, value ? 1U : 0U);
}

void mixDouble(std::uint64_t& hash, const double value) noexcept
{
    mixUint64(hash, std::bit_cast<std::uint64_t>(value));
}

void mixString(std::uint64_t& hash, const std::string_view value) noexcept
{
    mixUint64(hash, value.size());
    for (const char character : value) {
        mixByte(hash, static_cast<std::uint8_t>(character));
    }
}

void mixEpoch(std::uint64_t& hash, const AstronomicalEpoch& epoch) noexcept
{
    mixDouble(hash, epoch.julianDatePart1);
    mixDouble(hash, epoch.julianDatePart2);
    mixUint64(hash, static_cast<std::uint64_t>(epoch.timeScale));
}

void mixOptions(std::uint64_t& hash, const EphemerisEngineOptions& options) noexcept
{
    mixUint64(hash, static_cast<std::uint64_t>(options.engineKind));
    mixUint64(hash, static_cast<std::uint32_t>(options.correctionFlags));
    mixBool(hash, options.fallbackToSimpleEngine);
    mixBool(hash, options.enableAtmosphericRefraction);
    mixDouble(hash, options.atmosphericPressureHpa);
    mixDouble(hash, options.atmosphericTemperatureC);
    mixDouble(hash, options.relativeHumidity);
    mixDouble(hash, options.observingWavelengthMicrometers);
}

void mixObserver(std::uint64_t& hash, const core::GeoLocation& observer) noexcept
{
    mixDouble(hash, observer.latitudeDeg);
    mixDouble(hash, observer.longitudeDeg);
    mixDouble(hash, observer.elevationMeters);
}

void mixDateRange(std::uint64_t& hash, const EphemerisDateRange& range) noexcept
{
    mixString(hash, range.id);
    mixString(hash, range.displayName);
    mixEpoch(hash, range.start);
    mixEpoch(hash, range.end);
}

void mixOptionalDateRange(std::uint64_t& hash, const std::optional<EphemerisDateRange>& range) noexcept
{
    mixBool(hash, range.has_value());
    if (range.has_value()) {
        mixDateRange(hash, *range);
    }
}

void mixOptionalDouble(std::uint64_t& hash, const std::optional<double>& value) noexcept
{
    mixBool(hash, value.has_value());
    if (value.has_value()) {
        mixDouble(hash, *value);
    }
}

void mixOptionalEquatorial(std::uint64_t& hash, const std::optional<core::EquatorialCoordinate>& coordinate) noexcept
{
    mixBool(hash, coordinate.has_value());
    if (!coordinate.has_value()) {
        return;
    }

    mixDouble(hash, coordinate->rightAscensionHours);
    mixDouble(hash, coordinate->declinationDeg);
}

void mixOptionalAstrometry(std::uint64_t& hash, const std::optional<CatalogStarAstrometry>& astrometry) noexcept
{
    mixBool(hash, astrometry.has_value());
    if (!astrometry.has_value()) {
        return;
    }

    mixDouble(hash, astrometry->referenceEquatorial.rightAscensionHours);
    mixDouble(hash, astrometry->referenceEquatorial.declinationDeg);
    mixEpoch(hash, astrometry->referenceEpoch);
    mixOptionalDouble(hash, astrometry->properMotionRightAscensionMasPerYear);
    mixOptionalDouble(hash, astrometry->properMotionDeclinationMasPerYear);
    mixOptionalDouble(hash, astrometry->stellarParallaxMas);
    mixOptionalDouble(hash, astrometry->radialVelocityKmPerSecond);
    mixOptionalDateRange(hash, astrometry->validityRange);
}

void mixDeepSkyObject(std::uint64_t& hash, const std::optional<DeepSkyObjectInfo>& deepSkyObject) noexcept
{
    mixBool(hash, deepSkyObject.has_value());
    if (!deepSkyObject.has_value()) {
        return;
    }

    mixUint64(hash, static_cast<std::uint64_t>(deepSkyObject->kind));
    mixOptionalDouble(hash, deepSkyObject->majorAxisArcmin);
    mixOptionalDouble(hash, deepSkyObject->minorAxisArcmin);
    mixOptionalDouble(hash, deepSkyObject->positionAngleDeg);
    mixUint64(hash, deepSkyObject->aliases.size());
    for (const std::string& alias : deepSkyObject->aliases) {
        mixString(hash, alias);
    }
}

void mixCatalogBody(std::uint64_t& hash, const CelestialBody& body) noexcept
{
    mixString(hash, body.id);
    mixString(hash, body.displayName);
    mixUint64(hash, static_cast<std::uint64_t>(body.type));
    mixUint64(hash, static_cast<std::uint64_t>(body.ephemerisSource));
    mixDouble(hash, body.visualMagnitude);
    mixOptionalEquatorial(hash, body.fixedEquatorial);
    mixOptionalAstrometry(hash, body.starAstrometry);
    mixDeepSkyObject(hash, body.deepSkyObject);
}

[[nodiscard]] std::uint64_t hashRequest(const EphemerisRequest& request) noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    mixEpoch(hash, request.epoch);
    mixUint64(hash, static_cast<std::uint64_t>(request.context.utcTime.time_since_epoch().count()));
    mixObserver(hash, request.context.observer);
    mixOptions(hash, request.options);
    return hash;
}

[[nodiscard]] std::uint64_t hashCatalogBodies(const std::vector<CelestialBody>& catalogBodies) noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    mixUint64(hash, catalogBodies.size());
    for (const CelestialBody& body : catalogBodies) {
        mixCatalogBody(hash, body);
    }
    return hash;
}

[[nodiscard]] std::uint64_t hashDataSetInfo(const EphemerisDataSetInfo& dataSetInfo) noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    mixString(hash, dataSetInfo.id);
    mixString(hash, dataSetInfo.displayName);
    mixString(hash, dataSetInfo.version);
    mixString(hash, dataSetInfo.provenance);
    mixUint64(hash, dataSetInfo.dateRanges.size());
    for (const EphemerisDateRange& range : dataSetInfo.dateRanges) {
        mixDateRange(hash, range);
    }
    return hash;
}

void appendKeyPart(std::string& key, const std::string_view label, const std::uint64_t value)
{
    std::array<char, 16> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
    key.append(label);
    key.push_back(':');
    key.append(buffer.data(), result.ptr);
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
    std::string key;
    key.reserve(96);
    appendKeyPart(key, "request", hashRequest(request));
    key.push_back('|');
    appendKeyPart(key, "catalog", hashCatalogBodies(catalogBodies));
    key.push_back('|');
    appendKeyPart(key, "dataset", hashDataSetInfo(dataSetInfo));
    return key;
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
