#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "UtcTimeCodec.hpp"

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
    mixUint64(hash, static_cast<std::uint64_t>(options.engineKind()));
    mixUint64(hash, static_cast<std::uint32_t>(options.correctionFlags()));
    mixBool(hash, options.fallbackToSimpleEngine());
    mixBool(hash, options.enableAtmosphericRefraction());
    mixDouble(hash, options.atmosphericPressureHpa());
    mixDouble(hash, options.atmosphericTemperatureC());
    mixDouble(hash, options.relativeHumidity());
    mixDouble(hash, options.observingWavelengthMicrometers());
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
    mixUint64(hash, static_cast<std::uint64_t>(core::UtcTimeCodec::toEpochMicros(request.context.utcTime)));
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

[[nodiscard]] bool sameDoubleIdentity(const double lhs, const double rhs) noexcept
{
    return std::bit_cast<std::uint64_t>(lhs) == std::bit_cast<std::uint64_t>(rhs);
}

[[nodiscard]] bool sameEpoch(const AstronomicalEpoch& lhs, const AstronomicalEpoch& rhs) noexcept
{
    return sameDoubleIdentity(lhs.julianDatePart1, rhs.julianDatePart1)
           && sameDoubleIdentity(lhs.julianDatePart2, rhs.julianDatePart2) && lhs.timeScale == rhs.timeScale;
}

[[nodiscard]] bool sameObserver(const core::GeoLocation& lhs, const core::GeoLocation& rhs) noexcept
{
    return sameDoubleIdentity(lhs.latitudeDeg, rhs.latitudeDeg)
           && sameDoubleIdentity(lhs.longitudeDeg, rhs.longitudeDeg)
           && sameDoubleIdentity(lhs.elevationMeters, rhs.elevationMeters);
}

[[nodiscard]] bool sameOptions(const EphemerisEngineOptions& lhs, const EphemerisEngineOptions& rhs) noexcept
{
    return lhs.engineKind() == rhs.engineKind() && lhs.correctionFlags() == rhs.correctionFlags()
           && lhs.fallbackToSimpleEngine() == rhs.fallbackToSimpleEngine()
           && lhs.enableAtmosphericRefraction() == rhs.enableAtmosphericRefraction()
           && sameDoubleIdentity(lhs.atmosphericPressureHpa(), rhs.atmosphericPressureHpa())
           && sameDoubleIdentity(lhs.atmosphericTemperatureC(), rhs.atmosphericTemperatureC())
           && sameDoubleIdentity(lhs.relativeHumidity(), rhs.relativeHumidity())
           && sameDoubleIdentity(lhs.observingWavelengthMicrometers(), rhs.observingWavelengthMicrometers());
}

[[nodiscard]] bool sameRequest(const EphemerisRequest& lhs, const EphemerisRequest& rhs) noexcept
{
    return sameEpoch(lhs.epoch, rhs.epoch)
           && core::UtcTimeCodec::toEpochMicros(lhs.context.utcTime)
                  == core::UtcTimeCodec::toEpochMicros(rhs.context.utcTime)
           && sameObserver(lhs.context.observer, rhs.context.observer) && sameOptions(lhs.options, rhs.options);
}

[[nodiscard]] bool sameDateRange(const EphemerisDateRange& lhs, const EphemerisDateRange& rhs) noexcept
{
    return lhs.id == rhs.id && lhs.displayName == rhs.displayName && sameEpoch(lhs.start, rhs.start)
           && sameEpoch(lhs.end, rhs.end);
}

[[nodiscard]] bool sameOptionalDateRange(
    const std::optional<EphemerisDateRange>& lhs, const std::optional<EphemerisDateRange>& rhs
) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    return !lhs.has_value() || sameDateRange(*lhs, *rhs);
}

[[nodiscard]] bool sameOptionalDouble(const std::optional<double>& lhs, const std::optional<double>& rhs) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    return !lhs.has_value() || sameDoubleIdentity(*lhs, *rhs);
}

[[nodiscard]] bool sameOptionalEquatorial(
    const std::optional<core::EquatorialCoordinate>& lhs, const std::optional<core::EquatorialCoordinate>& rhs
) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    return !lhs.has_value()
           || (sameDoubleIdentity(lhs->rightAscensionHours, rhs->rightAscensionHours)
               && sameDoubleIdentity(lhs->declinationDeg, rhs->declinationDeg));
}

[[nodiscard]] bool sameAstrometry(const CatalogStarAstrometry& lhs, const CatalogStarAstrometry& rhs) noexcept
{
    return sameDoubleIdentity(lhs.referenceEquatorial.rightAscensionHours, rhs.referenceEquatorial.rightAscensionHours)
           && sameDoubleIdentity(lhs.referenceEquatorial.declinationDeg, rhs.referenceEquatorial.declinationDeg)
           && sameEpoch(lhs.referenceEpoch, rhs.referenceEpoch)
           && sameOptionalDouble(lhs.properMotionRightAscensionMasPerYear, rhs.properMotionRightAscensionMasPerYear)
           && sameOptionalDouble(lhs.properMotionDeclinationMasPerYear, rhs.properMotionDeclinationMasPerYear)
           && sameOptionalDouble(lhs.stellarParallaxMas, rhs.stellarParallaxMas)
           && sameOptionalDouble(lhs.radialVelocityKmPerSecond, rhs.radialVelocityKmPerSecond)
           && sameOptionalDateRange(lhs.validityRange, rhs.validityRange);
}

[[nodiscard]] bool sameOptionalAstrometry(
    const std::optional<CatalogStarAstrometry>& lhs, const std::optional<CatalogStarAstrometry>& rhs
) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    return !lhs.has_value() || sameAstrometry(*lhs, *rhs);
}

[[nodiscard]] bool sameDeepSkyObject(const DeepSkyObjectInfo& lhs, const DeepSkyObjectInfo& rhs)
{
    return lhs.kind == rhs.kind && lhs.aliases == rhs.aliases
           && sameOptionalDouble(lhs.majorAxisArcmin, rhs.majorAxisArcmin)
           && sameOptionalDouble(lhs.minorAxisArcmin, rhs.minorAxisArcmin)
           && sameOptionalDouble(lhs.positionAngleDeg, rhs.positionAngleDeg);
}

[[nodiscard]] bool
sameOptionalDeepSkyObject(const std::optional<DeepSkyObjectInfo>& lhs, const std::optional<DeepSkyObjectInfo>& rhs)
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    return !lhs.has_value() || sameDeepSkyObject(*lhs, *rhs);
}

[[nodiscard]] bool sameCatalogBody(const CelestialBody& lhs, const CelestialBody& rhs)
{
    return lhs.id == rhs.id && lhs.displayName == rhs.displayName && lhs.type == rhs.type
           && lhs.ephemerisSource == rhs.ephemerisSource && sameDoubleIdentity(lhs.visualMagnitude, rhs.visualMagnitude)
           && sameOptionalEquatorial(lhs.fixedEquatorial, rhs.fixedEquatorial)
           && sameOptionalAstrometry(lhs.starAstrometry, rhs.starAstrometry)
           && sameOptionalDeepSkyObject(lhs.deepSkyObject, rhs.deepSkyObject);
}

[[nodiscard]] bool sameCatalogBodies(const std::vector<CelestialBody>& lhs, const std::vector<CelestialBody>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t bodyIndex = 0U; bodyIndex < lhs.size(); ++bodyIndex) {
        if (!sameCatalogBody(lhs[bodyIndex], rhs[bodyIndex])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool sameDataSetInfo(const EphemerisDataSetInfo& lhs, const EphemerisDataSetInfo& rhs)
{
    if (lhs.id != rhs.id || lhs.displayName != rhs.displayName || lhs.version != rhs.version
        || lhs.provenance != rhs.provenance || lhs.dateRanges.size() != rhs.dateRanges.size()) {
        return false;
    }
    for (std::size_t rangeIndex = 0U; rangeIndex < lhs.dateRanges.size(); ++rangeIndex) {
        if (!sameDateRange(lhs.dateRanges[rangeIndex], rhs.dateRanges[rangeIndex])) {
            return false;
        }
    }
    return true;
}

}  // namespace

EphemerisComputationCache::EphemerisComputationCache(const std::size_t maxEntries)
    : m_maxEntries(maxEntries == 0U ? 1U : maxEntries)
{
}

EphemerisComputationCache::RequestIdentity EphemerisComputationCache::makeIdentity(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo
)
{
    return RequestIdentity{
        .request = request,
        .catalogBodies = catalogBodies,
        .dataSetInfo = dataSetInfo,
    };
}

bool EphemerisComputationCache::matchesIdentity(
    const EphemerisComputationCache::RequestIdentity& identity,
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo
)
{
    return sameRequest(identity.request, request) && sameCatalogBodies(identity.catalogBodies, catalogBodies)
           && sameDataSetInfo(identity.dataSetInfo, dataSetInfo);
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
    if (!matchesIdentity(snapshot->second.identity, request, catalogBodies, dataSetInfo)) {
        return std::nullopt;
    }

    return snapshot->second.snapshot;
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
    m_snapshots[key] = SnapshotEntry{
        .identity = makeIdentity(request, catalogBodies, dataSetInfo),
        .snapshot = snapshot,
    };

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
    if (!matchesIdentity(preparedState->second.identity, request, catalogBodies, dataSetInfo)) {
        return nullptr;
    }

    return preparedState->second.preparedState;
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
    m_preparedStates[key] = PreparedStateEntry{
        .identity = makeIdentity(request, catalogBodies, dataSetInfo),
        .preparedState = std::move(preparedState),
    };

    while (m_preparedStateOrder.size() > m_maxEntries) {
        m_preparedStates.erase(m_preparedStateOrder.front());
        m_preparedStateOrder.pop_front();
    }
}

std::optional<CelestialBodyState> EphemerisComputationCache::findBodyState(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo,
    const std::size_t bodyIndex
) const
{
    const std::string key = makeBodyStateKey(request, catalogBodies, dataSetInfo, bodyIndex);

    const std::scoped_lock lock(m_mutex);
    const auto bodyState = m_bodyStates.find(key);
    if (bodyState == m_bodyStates.end()) {
        return std::nullopt;
    }
    if (bodyState->second.bodyIndex != bodyIndex
        || !matchesIdentity(bodyState->second.identity, request, catalogBodies, dataSetInfo)) {
        return std::nullopt;
    }

    return bodyState->second.state;
}

void EphemerisComputationCache::storeBodyState(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo,
    const std::size_t bodyIndex,
    const CelestialBodyState& state
) const
{
    const std::string key = makeBodyStateKey(request, catalogBodies, dataSetInfo, bodyIndex);

    const std::scoped_lock lock(m_mutex);
    if (!m_bodyStates.contains(key)) {
        m_bodyStateOrder.push_back(key);
    }
    m_bodyStates[key] = BodyStateEntry{
        .identity = makeIdentity(request, catalogBodies, dataSetInfo),
        .bodyIndex = bodyIndex,
        .state = state,
    };

    while (m_bodyStateOrder.size() > m_maxEntries) {
        m_bodyStates.erase(m_bodyStateOrder.front());
        m_bodyStateOrder.pop_front();
    }
}

void EphemerisComputationCache::clear() const
{
    const std::scoped_lock lock(m_mutex);
    m_snapshotOrder.clear();
    m_snapshots.clear();
    m_preparedStateOrder.clear();
    m_preparedStates.clear();
    m_bodyStateOrder.clear();
    m_bodyStates.clear();
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

std::string EphemerisComputationCache::makeBodyStateKey(
    const EphemerisRequest& request,
    const std::vector<CelestialBody>& catalogBodies,
    const EphemerisDataSetInfo& dataSetInfo,
    const std::size_t bodyIndex
)
{
    std::string key = "body|";
    key += makeRequestKey(request, catalogBodies, dataSetInfo);
    key.push_back('|');
    appendKeyPart(key, "index", bodyIndex);
    return key;
}

}  // namespace skygate::ephemeris::highprecision
