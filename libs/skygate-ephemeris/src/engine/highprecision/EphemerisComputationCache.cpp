#include "EphemerisComputationCache.hpp"
#include "PreparedEphemerisRequestState.hpp"
#include "UtcTimeCodec.hpp"
#include "hash/Fnv1a64Hasher.hpp"
#include "hash/HashConstants.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris::highprecision {
namespace {

void mixUint64(std::uint64_t& hash, const std::uint64_t value) noexcept
{
    hash = skygate::core::Fnv1a64Hasher::combine(hash, value);
}

void mixBool(std::uint64_t& hash, const bool value) noexcept
{
    mixUint64(hash, value ? 1U : 0U);
}

void mixDouble(std::uint64_t& hash, const double value) noexcept
{
    mixUint64(hash, std::bit_cast<std::uint64_t>(value));
}

void mixString(std::uint64_t& hash, const std::string_view value) noexcept
{
    mixUint64(hash, value.size());
    hash = skygate::core::Fnv1a64Hasher::hashFrom(hash, value);
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
    std::uint64_t hash = skygate::core::HashConstants::kFnv1a64OffsetBasis;
    mixEpoch(hash, request.epoch);
    mixUint64(hash, static_cast<std::uint64_t>(skygate::core::UtcTimeCodec::toEpochMicros(request.context.utcTime)));
    mixObserver(hash, request.context.observer);
    mixOptions(hash, request.options);
    return hash;
}

[[nodiscard]] std::uint64_t hashDataSetInfo(const EphemerisDatasetInfo& dataSetInfo) noexcept
{
    std::uint64_t hash = skygate::core::HashConstants::kFnv1a64OffsetBasis;
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

[[nodiscard]] bool sameDataSetInfo(const EphemerisDatasetInfo& lhs, const EphemerisDatasetInfo& rhs)
{
    if (lhs.id != rhs.id || lhs.displayName != rhs.displayName || lhs.version != rhs.version
        || lhs.provenance != rhs.provenance || lhs.dateRanges.size() != rhs.dateRanges.size()) {
        return false;
    }
    for (std::size_t rangeIndex = 0U; rangeIndex < lhs.dateRanges.size(); ++rangeIndex) {
        const EphemerisDateRange& leftRange = lhs.dateRanges[rangeIndex];
        const EphemerisDateRange& rightRange = rhs.dateRanges[rangeIndex];
        if (leftRange.id != rightRange.id || leftRange.displayName != rightRange.displayName
            || !leftRange.start.isIdenticalTo(rightRange.start) || !leftRange.end.isIdenticalTo(rightRange.end)) {
            return false;
        }
    }
    return true;
}

}  // namespace

EphemerisComputationCache::EphemerisComputationCache(const std::size_t maxEntries)
    : m_snapshots(maxEntries), m_preparedStates(maxEntries), m_bodyStates(maxEntries)
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
    const EphemerisRequest& cachedRequest = identity.request;
    return identity.catalogIdentity == catalogIdentity && cachedRequest.epoch.isIdenticalTo(request.epoch)
           && skygate::core::UtcTimeCodec::toEpochMicros(cachedRequest.context.utcTime)
                  == skygate::core::UtcTimeCodec::toEpochMicros(request.context.utcTime)
           && cachedRequest.context.observer.isIdenticalTo(request.context.observer)
           && cachedRequest.options.isIdenticalTo(request.options)
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
    const SnapshotEntry* entry = m_snapshots.find(key);
    if (entry == nullptr || !matchesIdentity(entry->identity, request, catalogIdentity, dataSetInfo)) {
        return std::nullopt;
    }

    return entry->snapshot;
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
    const SnapshotEntry* entry = m_snapshots.find(key);
    if (entry == nullptr || !matchesIdentity(entry->identity, request, catalogIdentity, dataSetInfo)) {
        return std::nullopt;
    }
    if (bodyIndex >= entry->snapshot.states.size()) {
        return std::nullopt;
    }

    return entry->snapshot.states[bodyIndex];
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
    m_snapshots.store(
        key,
        SnapshotEntry{
            .identity = makeIdentity(request, catalogIdentity, dataSetInfo),
            .snapshot = snapshot,
        }
    );
}

std::shared_ptr<const PreparedEphemerisRequestState> EphemerisComputationCache::findPreparedRequestState(
    const EphemerisRequest& request,
    const CelestialBodyCatalog* catalogIdentity,
    const EphemerisDatasetInfo& dataSetInfo
) const
{
    const std::string key = makePreparedStateKey(request, catalogIdentity, dataSetInfo);

    const std::scoped_lock lock(m_mutex);
    const PreparedStateEntry* entry = m_preparedStates.find(key);
    if (entry == nullptr || !matchesIdentity(entry->identity, request, catalogIdentity, dataSetInfo)) {
        return nullptr;
    }

    return entry->preparedState;
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
    m_preparedStates.store(
        key,
        PreparedStateEntry{
            .identity = makeIdentity(request, catalogIdentity, dataSetInfo),
            .preparedState = std::move(preparedState),
        }
    );
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
    const BodyStateEntry* entry = m_bodyStates.find(key);
    if (entry == nullptr || entry->bodyIndex != bodyIndex
        || !matchesIdentity(entry->identity, request, catalogIdentity, dataSetInfo)) {
        return std::nullopt;
    }

    return entry->state;
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
    m_bodyStates.store(
        key,
        BodyStateEntry{
            .identity = makeIdentity(request, catalogIdentity, dataSetInfo),
            .bodyIndex = bodyIndex,
            .state = state,
        }
    );
}

void EphemerisComputationCache::clear() const
{
    const std::scoped_lock lock(m_mutex);
    m_snapshots.clear();
    m_preparedStates.clear();
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
