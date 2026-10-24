#include "catalog/CatalogBinaryCodec.hpp"

#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "DistantCelestialBody.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogStarAstrometry.hpp"
#include "catalog/InMemoryStarCatalog.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"
#include "time/TimeScaleCodec.hpp"
#include "catalog/normalize/CatalogSnapshotValidator.hpp"

#include <QDataStream>
#include <QIODevice>
#include <QString>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

constexpr std::uint32_t kBinaryCatalogMagic = 0x53474243U;  // "SGBC"
constexpr std::uint64_t kMaxBodyCount = 5'000'000ULL;
constexpr std::uint64_t kMaxOrderEntryCount = 10'000'000ULL;

// Reads an element count and rejects a value above `limit`, so a corrupt
// payload cannot reserve more than the codec is willing to hold.
[[nodiscard]] bool readCount(QDataStream& stream, std::uint64_t& count, const std::uint64_t limit)
{
    stream >> count;
    return stream.status() == QDataStream::Ok && count <= limit;
}

void writeString(QDataStream& stream, const std::string& value)
{
    stream << QString::fromStdString(value);
}

[[nodiscard]] bool readString(QDataStream& stream, std::string& value)
{
    QString text;
    stream >> text;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    value = text.toStdString();
    return true;
}

void writeStringList(QDataStream& stream, const std::vector<std::string>& values)
{
    stream << static_cast<std::uint64_t>(values.size());
    for (const std::string& value : values) {
        writeString(stream, value);
    }
}

// Reads a length-prefixed string list. `rejectEmpty` marks a field whose empty
// entries carry no meaning, so a payload holding one is rejected outright.
[[nodiscard]] bool readStringList(QDataStream& stream, std::vector<std::string>& values, const bool rejectEmpty)
{
    values.clear();
    std::uint64_t count = 0U;
    if (!readCount(stream, count, kMaxBodyCount)) {
        return false;
    }

    values.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        std::string value;
        if (!readString(stream, value) || (rejectEmpty && value.empty())) {
            return false;
        }
        values.push_back(std::move(value));
    }
    return true;
}

// A presence flag followed by the value when it is present, which keeps an
// absent value distinct from a default-constructed one.
template <typename T, typename Write>
void writeOptional(QDataStream& stream, const std::optional<T>& value, Write write)
{
    stream << value.has_value();
    if (value.has_value()) {
        write(*value);
    }
}

template <typename T, typename Read>
[[nodiscard]] bool readOptional(QDataStream& stream, std::optional<T>& value, Read read)
{
    bool hasValue = false;
    stream >> hasValue;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    if (!hasValue) {
        value = std::nullopt;
        return true;
    }

    T parsed{};
    if (!read(parsed)) {
        return false;
    }
    value = std::move(parsed);
    return true;
}

void writeOptionalDouble(QDataStream& stream, const std::optional<double>& value)
{
    writeOptional(stream, value, [&stream](const double scalar) { stream << scalar; });
}

[[nodiscard]] bool readOptionalDouble(QDataStream& stream, std::optional<double>& value)
{
    return readOptional(stream, value, [&stream](double& scalar) {
        stream >> scalar;
        return stream.status() == QDataStream::Ok;
    });
}

void writeIdentifiers(QDataStream& stream, const std::vector<CatalogIdentifier>& identifiers)
{
    stream << static_cast<std::uint64_t>(identifiers.size());
    for (const CatalogIdentifier& identifier : identifiers) {
        writeString(stream, identifier.namespaceName);
        writeString(stream, identifier.value);
    }
}

[[nodiscard]] bool readIdentifiers(QDataStream& stream, std::vector<CatalogIdentifier>& identifiers)
{
    identifiers.clear();
    std::uint64_t identifierCount = 0U;
    if (!readCount(stream, identifierCount, kMaxBodyCount)) {
        return false;
    }

    identifiers.reserve(static_cast<std::size_t>(identifierCount));
    for (std::uint64_t index = 0; index < identifierCount; ++index) {
        CatalogIdentifier identifier;
        if (!readString(stream, identifier.namespaceName) || !readString(stream, identifier.value)
            || identifier.namespaceName.empty() || identifier.value.empty()) {
            return false;
        }
        identifiers.push_back(std::move(identifier));
    }
    return true;
}

void writeObjectIdentity(QDataStream& stream, const CatalogObjectIdentity& identity)
{
    writeString(stream, identity.sourceRecordId);
    stream << static_cast<std::uint8_t>(identity.idScope);
    writeIdentifiers(stream, identity.externalIdentifiers);
    writeStringList(stream, identity.aliases);
    writeStringList(stream, identity.retainedCanonicalIds);
}

[[nodiscard]] bool readObjectIdentity(QDataStream& stream, CatalogObjectIdentity& identity)
{
    if (!readString(stream, identity.sourceRecordId)) {
        return false;
    }

    std::uint8_t idScope = 0U;
    stream >> idScope;
    if (stream.status() != QDataStream::Ok
        || !CatalogSnapshotValidator::isKnownIdScope(static_cast<CatalogObjectIdentity::IdScope>(idScope))) {
        return false;
    }
    identity.idScope = static_cast<CatalogObjectIdentity::IdScope>(idScope);

    return readIdentifiers(stream, identity.externalIdentifiers) && readStringList(stream, identity.aliases, false)
           && readStringList(stream, identity.retainedCanonicalIds, true);
}

void writeEpoch(QDataStream& stream, const skygate::core::AstronomicalEpoch& epoch)
{
    stream << epoch.julianDatePart1 << epoch.julianDatePart2
           << static_cast<std::uint8_t>(skygate::core::TimeScaleCodec::toRaw(epoch.timeScale));
}

[[nodiscard]] bool readEpoch(QDataStream& stream, skygate::core::AstronomicalEpoch& epoch)
{
    std::uint8_t timeScale = 0U;
    stream >> epoch.julianDatePart1 >> epoch.julianDatePart2 >> timeScale;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    const std::optional<skygate::core::TimeScale> parsedTimeScale =
        skygate::core::TimeScaleCodec::fromRaw(static_cast<int>(timeScale));
    if (!parsedTimeScale.has_value()) {
        return false;
    }
    epoch.timeScale = *parsedTimeScale;
    return true;
}

void writeEquatorial(QDataStream& stream, const skygate::core::EquatorialCoordinate& coordinate)
{
    stream << coordinate.rightAscensionHours << coordinate.declinationDeg;
}

[[nodiscard]] bool readEquatorial(QDataStream& stream, skygate::core::EquatorialCoordinate& coordinate)
{
    stream >> coordinate.rightAscensionHours >> coordinate.declinationDeg;
    return stream.status() == QDataStream::Ok;
}

void writeDateRange(QDataStream& stream, const EphemerisDateRange& range)
{
    writeString(stream, range.id);
    writeString(stream, range.displayName);
    writeEpoch(stream, range.start);
    writeEpoch(stream, range.end);
}

[[nodiscard]] bool readDateRange(QDataStream& stream, EphemerisDateRange& range)
{
    return readString(stream, range.id) && readString(stream, range.displayName) && readEpoch(stream, range.start)
           && readEpoch(stream, range.end);
}

void writeStarAstrometry(QDataStream& stream, const CatalogStarAstrometry& astrometry)
{
    writeEquatorial(stream, astrometry.referenceEquatorial);
    writeEpoch(stream, astrometry.referenceEpoch);
    writeOptionalDouble(stream, astrometry.properMotionRightAscensionMasPerYear);
    writeOptionalDouble(stream, astrometry.properMotionDeclinationMasPerYear);
    writeOptionalDouble(stream, astrometry.stellarParallaxMas);
    writeOptionalDouble(stream, astrometry.radialVelocityKmPerSecond);
    writeOptional(stream, astrometry.validityRange, [&stream](const EphemerisDateRange& range) {
        writeDateRange(stream, range);
    });
}

[[nodiscard]] bool readStarAstrometry(QDataStream& stream, CatalogStarAstrometry& astrometry)
{
    if (!readEquatorial(stream, astrometry.referenceEquatorial) || !readEpoch(stream, astrometry.referenceEpoch)) {
        return false;
    }
    if (!readOptionalDouble(stream, astrometry.properMotionRightAscensionMasPerYear)
        || !readOptionalDouble(stream, astrometry.properMotionDeclinationMasPerYear)
        || !readOptionalDouble(stream, astrometry.stellarParallaxMas)
        || !readOptionalDouble(stream, astrometry.radialVelocityKmPerSecond)) {
        return false;
    }
    return readOptional(stream, astrometry.validityRange, [&stream](EphemerisDateRange& range) {
        return readDateRange(stream, range);
    });
}

void writeDeepSkyInfo(QDataStream& stream, const DeepSkyObjectInfo& info)
{
    stream << static_cast<std::uint8_t>(info.kind);
    writeStringList(stream, info.aliases);
    writeOptionalDouble(stream, info.majorAxisArcmin);
    writeOptionalDouble(stream, info.minorAxisArcmin);
    writeOptionalDouble(stream, info.positionAngleDeg);
}

[[nodiscard]] bool readDeepSkyInfo(QDataStream& stream, DeepSkyObjectInfo& info)
{
    std::uint8_t kind = 0U;
    stream >> kind;
    if (stream.status() != QDataStream::Ok
        || !CatalogSnapshotValidator::isKnownDeepSkyObjectKind(static_cast<DeepSkyObjectInfo::Kind>(kind))) {
        return false;
    }
    info.kind = static_cast<DeepSkyObjectInfo::Kind>(kind);

    return readStringList(stream, info.aliases, false) && readOptionalDouble(stream, info.majorAxisArcmin)
           && readOptionalDouble(stream, info.minorAxisArcmin) && readOptionalDouble(stream, info.positionAngleDeg);
}

}  // namespace

void CatalogBinaryCodec::writeBodyHeader(
    QDataStream& stream,
    const BaseCelestialBody& body,
    const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial
)
{
    writeString(stream, body.id);
    writeString(stream, body.displayName);
    stream << static_cast<std::uint8_t>(body.kind) << body.visualMagnitude;
    writeObjectIdentity(stream, body.identity);
    writeOptional(stream, fixedEquatorial, [&stream](const skygate::core::EquatorialCoordinate& coordinate) {
        writeEquatorial(stream, coordinate);
    });
}

bool CatalogBinaryCodec::readBodyHeader(
    QDataStream& stream, BaseCelestialBody& body, std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial
)
{
    if (!readString(stream, body.id) || !readString(stream, body.displayName)) {
        return false;
    }

    std::uint8_t kind = 0U;
    stream >> kind >> body.visualMagnitude;
    if (stream.status() != QDataStream::Ok
        || !CatalogSnapshotValidator::isKnownBodyKind(static_cast<BaseCelestialBody::Kind>(kind))) {
        return false;
    }
    body.kind = static_cast<BaseCelestialBody::Kind>(kind);

    return readObjectIdentity(stream, body.identity)
           && readOptional(stream, fixedEquatorial, [&stream](skygate::core::EquatorialCoordinate& coordinate) {
                  return readEquatorial(stream, coordinate);
              });
}

QByteArray CatalogBinaryCodec::serialize(const CelestialBodyCatalog& catalog)
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << kBinaryCatalogMagic << kSchemaVersion;

    const auto ownGalaxyBodies = catalog.ownGalaxyBodies();
    stream << static_cast<std::uint64_t>(ownGalaxyBodies.size());
    for (const OwnGalaxyCelestialBody& body : ownGalaxyBodies) {
        writeBodyHeader(stream, body, body.fixedEquatorial);
        writeOptional(stream, body.starAstrometry, [&stream](const CatalogStarAstrometry& astrometry) {
            writeStarAstrometry(stream, astrometry);
        });
    }

    const auto distantBodies = catalog.distantBodies();
    stream << static_cast<std::uint64_t>(distantBodies.size());
    for (const DistantCelestialBody& body : distantBodies) {
        writeBodyHeader(stream, body, body.fixedEquatorial);
        writeOptional(stream, body.deepSkyObject, [&stream](const DeepSkyObjectInfo& info) {
            writeDeepSkyInfo(stream, info);
        });
    }

    const auto orderedIndexes = catalog.orderedBodyIndexes();
    stream << static_cast<std::uint64_t>(orderedIndexes.size());
    for (const CelestialBodyCatalog::OrderEntry& entry : orderedIndexes) {
        stream << static_cast<std::uint8_t>(entry.domain) << static_cast<std::uint64_t>(entry.bodyIndex);
    }

    return buffer;
}

std::unique_ptr<IStarCatalog> CatalogBinaryCodec::deserialize(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_5);
    stream.setByteOrder(QDataStream::LittleEndian);

    std::uint32_t magic = 0U;
    std::uint16_t schemaVersion = 0U;
    stream >> magic >> schemaVersion;
    if (stream.status() != QDataStream::Ok || magic != kBinaryCatalogMagic || schemaVersion != kSchemaVersion) {
        return nullptr;
    }

    std::uint64_t ownGalaxyCount = 0U;
    if (!readCount(stream, ownGalaxyCount, kMaxBodyCount)) {
        return nullptr;
    }
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    ownGalaxyBodies.reserve(static_cast<std::size_t>(ownGalaxyCount));
    for (std::uint64_t index = 0; index < ownGalaxyCount; ++index) {
        OwnGalaxyCelestialBody body;
        if (!readBodyHeader(stream, body, body.fixedEquatorial)
            || !readOptional(stream, body.starAstrometry, [&stream](CatalogStarAstrometry& astrometry) {
                   return readStarAstrometry(stream, astrometry);
               })) {
            return nullptr;
        }
        ownGalaxyBodies.push_back(std::move(body));
    }

    std::uint64_t distantCount = 0U;
    if (!readCount(stream, distantCount, kMaxBodyCount)) {
        return nullptr;
    }
    std::vector<DistantCelestialBody> distantBodies;
    distantBodies.reserve(static_cast<std::size_t>(distantCount));
    for (std::uint64_t index = 0; index < distantCount; ++index) {
        DistantCelestialBody body;
        if (!readBodyHeader(stream, body, body.fixedEquatorial)
            || !readOptional(stream, body.deepSkyObject, [&stream](DeepSkyObjectInfo& info) {
                   return readDeepSkyInfo(stream, info);
               })) {
            return nullptr;
        }
        distantBodies.push_back(std::move(body));
    }

    std::uint64_t orderEntryCount = 0U;
    if (!readCount(stream, orderEntryCount, kMaxOrderEntryCount)) {
        return nullptr;
    }
    std::vector<CelestialBodyCatalog::OrderEntry> orderedIndexes;
    orderedIndexes.reserve(static_cast<std::size_t>(orderEntryCount));
    for (std::uint64_t index = 0; index < orderEntryCount; ++index) {
        std::uint8_t domain = 0U;
        std::uint64_t bodyIndex = 0U;
        stream >> domain >> bodyIndex;
        if (stream.status() != QDataStream::Ok) {
            return nullptr;
        }
        const auto bodyDomain = static_cast<CelestialBodyCatalog::BodyDomain>(domain);
        if (!CatalogSnapshotValidator::isOrderEntryValid(
                bodyDomain, static_cast<std::size_t>(bodyIndex), ownGalaxyBodies.size(), distantBodies.size()
            )) {
            return nullptr;
        }
        orderedIndexes.push_back({
            .domain = bodyDomain,
            .bodyIndex = static_cast<std::size_t>(bodyIndex),
        });
    }
    if (!stream.atEnd()) {
        return nullptr;
    }

    const CatalogSnapshotValidator::Report report =
        CatalogSnapshotValidator::validate(ownGalaxyBodies, distantBodies, orderedIndexes);
    if (!report.ok) {
        return nullptr;
    }

    return std::make_unique<InMemoryStarCatalog>(
        CelestialBodyCatalog(std::move(ownGalaxyBodies), std::move(distantBodies), std::move(orderedIndexes))
    );
}

}  // namespace skygate::ephemeris
