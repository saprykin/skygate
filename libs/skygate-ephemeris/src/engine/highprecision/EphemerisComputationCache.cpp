#include "EphemerisComputationCache.hpp"
#include "PreparedEphemerisRequestState.hpp"
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

void mixEpoch(std::uint64_t& hash, const skygate::core::AstronomicalEpoch& epoch) noexcept
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

void mixObserver(std::uint64_t& hash, const skygate::core::GeoLocation& observer) noexcept
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

[[nodiscard]] std::uint64_t hashRequest(const EphemerisRequest& request) noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    mixEpoch(hash, request.epoch);
    mixUint64(hash, static_cast<std::uint64_t>(skygate::core::UtcTimeCodec::toEpochMicros(request.context.utcTime)));
    mixObserver(hash, request.context.observer);
    mixOptions(hash, request.options);
    return hash;
}

[[nodiscard]] std::uint64_t hashDataSetInfo(const EphemerisDatasetInfo& dataSetInfo) noexcept
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

[[nodiscard]] bool
sameEpoch(const skygate::core::AstronomicalEpoch& lhs, const skygate::core::AstronomicalEpoch& rhs) noexcept
{
    return sameDoubleIdentity(lhs.julianDatePart1, rhs.julianDatePart1)
           && sameDoubleIdentity(lhs.julianDatePart2, rhs.julianDatePart2) && lhs.timeScale == rhs.timeScale;
}

[[nodiscard]] bool sameObserver(const skygate::core::GeoLocation& lhs, const skygate::core::GeoLocation& rhs) noexcept
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
           && skygate::core::UtcTimeCodec::toEpochMicros(lhs.context.utcTime)
                  == skygate::core::UtcTimeCodec::toEpochMicros(rhs.context.utcTime)
           && sameObserver(lhs.context.observer, rhs.context.observer) && sameOptions(lhs.options, rhs.options);
}

[[nodiscard]] bool sameDateRange(const EphemerisDateRange& lhs, const EphemerisDateRange& rhs) noexcept
{
    return lhs.id == rhs.id && lhs.displayName == rhs.displayName && sameEpoch(lhs.start, rhs.start)
           && sameEpoch(lhs.end, rhs.end);
}

[[nodiscard]] bool sameDataSetInfo(const EphemerisDatasetInfo& lhs, const EphemerisDatasetInfo& rhs)
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
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo
)
{
    return RequestIdentity{
        .request = request,
        .catalogIdentity = catalogIdentity,
        .dataSetInfo = dataSetInfo,
    };
}

bool EphemerisComputationCache::matchesIdentity(
    const EphemerisComputationCache::RequestIdentity& identity,
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo
)
{
    return sameRequest(identity.request, request) && identity.catalogIdentity == catalogIdentity
           && sameDataSetInfo(identity.dataSetInfo, dataSetInfo);
}

std::optional<EphemerisSnapshot> EphemerisComputationCache::findSnapshot(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo
) const
{
    const std::string key = makeSnapshotKey(request, catalogIdentity, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    const auto snapshot = m_snapshots.find(key);
    if (snapshot == m_snapshots.end()) {
        return std::nullopt;
    }
    if (!matchesIdentity(snapshot->second.identity, request, catalogIdentity, dataSetInfo)) {
        return std::nullopt;
    }

    return snapshot->second.snapshot;
}

std::optional<CelestialBodyState> EphemerisComputationCache::findSnapshotBodyState(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo,
    const std::size_t bodyIndex
) const
{
    const std::string key = makeSnapshotKey(request, catalogIdentity, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    const auto snapshot = m_snapshots.find(key);
    if (snapshot == m_snapshots.end()) {
        return std::nullopt;
    }
    if (!matchesIdentity(snapshot->second.identity, request, catalogIdentity, dataSetInfo)) {
        return std::nullopt;
    }
    if (bodyIndex >= snapshot->second.snapshot.states.size()) {
        return std::nullopt;
    }

    return snapshot->second.snapshot.states[bodyIndex];
}

void EphemerisComputationCache::storeSnapshot(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo,
    const EphemerisSnapshot& snapshot
) const
{
    const std::string key = makeSnapshotKey(request, catalogIdentity, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    if (!m_snapshots.contains(key)) {
        m_snapshotOrder.push_back(key);
    }
    m_snapshots[key] = SnapshotEntry{
        .identity = makeIdentity(request, catalogIdentity, dataSetInfo),
        .snapshot = snapshot,
    };

    while (m_snapshotOrder.size() > m_maxEntries) {
        m_snapshots.erase(m_snapshotOrder.front());
        m_snapshotOrder.pop_front();
    }
}

std::shared_ptr<const PreparedEphemerisRequestState> EphemerisComputationCache::findPreparedRequestState(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo
) const
{
    const std::string key = makePreparedStateKey(request, catalogIdentity, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    const auto preparedState = m_preparedStates.find(key);
    if (preparedState == m_preparedStates.end()) {
        return nullptr;
    }
    if (!matchesIdentity(preparedState->second.identity, request, catalogIdentity, dataSetInfo)) {
        return nullptr;
    }

    return preparedState->second.preparedState;
}

void EphemerisComputationCache::storePreparedRequestState(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedState
) const
{
    if (preparedState == nullptr) {
        return;
    }

    const std::string key = makePreparedStateKey(request, catalogIdentity, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    if (!m_preparedStates.contains(key)) {
        m_preparedStateOrder.push_back(key);
    }
    m_preparedStates[key] = PreparedStateEntry{
        .identity = makeIdentity(request, catalogIdentity, dataSetInfo),
        .preparedState = std::move(preparedState),
    };

    while (m_preparedStateOrder.size() > m_maxEntries) {
        m_preparedStates.erase(m_preparedStateOrder.front());
        m_preparedStateOrder.pop_front();
    }
}

std::optional<CelestialBodyState> EphemerisComputationCache::findBodyState(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo,
    const std::size_t bodyIndex
) const
{
    const std::string key = makeBodyStateKey(request, catalogIdentity, dataSetInfo, bodyIndex);

    const std::scoped_lock lock(m_mutex);
    const auto bodyState = m_bodyStates.find(key);
    if (bodyState == m_bodyStates.end()) {
        return std::nullopt;
    }
    if (bodyState->second.bodyIndex != bodyIndex
        || !matchesIdentity(bodyState->second.identity, request, catalogIdentity, dataSetInfo)) {
        return std::nullopt;
    }

    return bodyState->second.state;
}

void EphemerisComputationCache::storeBodyState(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo,
    const std::size_t bodyIndex,
    const CelestialBodyState& state
) const
{
    const std::string key = makeBodyStateKey(request, catalogIdentity, dataSetInfo, bodyIndex);

    const std::scoped_lock lock(m_mutex);
    if (!m_bodyStates.contains(key)) {
        m_bodyStateOrder.push_back(key);
    }
    m_bodyStates[key] = BodyStateEntry{
        .identity = makeIdentity(request, catalogIdentity, dataSetInfo),
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
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo
)
{
    std::string key;
    key.reserve(96);
    appendKeyPart(key, "request", hashRequest(request));
    key.push_back('|');
    appendKeyPart(key, "catalog", reinterpret_cast<std::uintptr_t>(catalogIdentity));
    key.push_back('|');
    appendKeyPart(key, "dataset", hashDataSetInfo(dataSetInfo));
    return key;
}

std::string EphemerisComputationCache::makeSnapshotKey(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo
)
{
    return "snapshot|" + makeRequestKey(request, catalogIdentity, dataSetInfo);
}

std::string EphemerisComputationCache::makePreparedStateKey(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo
)
{
    return "prepared|" + makeRequestKey(request, catalogIdentity, dataSetInfo);
}

std::string EphemerisComputationCache::makeBodyStateKey(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo,
    const std::size_t bodyIndex
)
{
    std::string key = "body|";
    key += makeRequestKey(request, catalogIdentity, dataSetInfo);
    key.push_back('|');
    appendKeyPart(key, "index", bodyIndex);
    return key;
}

}  // namespace skygate::ephemeris::highprecision
