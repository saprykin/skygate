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

void writeOptionalDouble(QDataStream& stream, const std::optional<double>& value)
{
    stream << value.has_value();
    if (value.has_value()) {
        stream << *value;
    }
}

[[nodiscard]] bool readOptionalDouble(QDataStream& stream, std::optional<double>& value)
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
    double rawValue = 0.0;
    stream >> rawValue;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    value = rawValue;
    return true;
}

void writeObjectIdentity(QDataStream& stream, const CatalogObjectIdentity& identity)
{
    writeString(stream, identity.sourceRecordId);
    stream << static_cast<std::uint8_t>(identity.idScope);
    stream << static_cast<std::uint64_t>(identity.externalIdentifiers.size());
    for (const CatalogIdentifier& identifier : identity.externalIdentifiers) {
        writeString(stream, identifier.namespaceName);
        writeString(stream, identifier.value);
    }
    stream << static_cast<std::uint64_t>(identity.aliases.size());
    for (const std::string& alias : identity.aliases) {
        writeString(stream, alias);
    }
    stream << static_cast<std::uint64_t>(identity.retainedCanonicalIds.size());
    for (const std::string& retainedId : identity.retainedCanonicalIds) {
        writeString(stream, retainedId);
    }
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

    std::uint64_t identifierCount = 0U;
    stream >> identifierCount;
    if (stream.status() != QDataStream::Ok || identifierCount > kMaxBodyCount) {
        return false;
    }
    identity.externalIdentifiers.clear();
    identity.externalIdentifiers.reserve(static_cast<std::size_t>(identifierCount));
    for (std::uint64_t index = 0; index < identifierCount; ++index) {
        CatalogIdentifier identifier;
        if (!readString(stream, identifier.namespaceName) || !readString(stream, identifier.value)) {
            return false;
        }
        if (identifier.namespaceName.empty() || identifier.value.empty()) {
            return false;
        }
        identity.externalIdentifiers.push_back(std::move(identifier));
    }

    std::uint64_t aliasCount = 0U;
    stream >> aliasCount;
    if (stream.status() != QDataStream::Ok || aliasCount > kMaxBodyCount) {
        return false;
    }
    identity.aliases.clear();
    identity.aliases.reserve(static_cast<std::size_t>(aliasCount));
    for (std::uint64_t index = 0; index < aliasCount; ++index) {
        std::string alias;
        if (!readString(stream, alias)) {
            return false;
        }
        identity.aliases.push_back(std::move(alias));
    }

    std::uint64_t retainedCanonicalIdCount = 0U;
    stream >> retainedCanonicalIdCount;
    if (stream.status() != QDataStream::Ok || retainedCanonicalIdCount > kMaxBodyCount) {
        return false;
    }
    identity.retainedCanonicalIds.clear();
    identity.retainedCanonicalIds.reserve(static_cast<std::size_t>(retainedCanonicalIdCount));
    for (std::uint64_t index = 0; index < retainedCanonicalIdCount; ++index) {
        std::string retainedId;
        if (!readString(stream, retainedId) || retainedId.empty()) {
            return false;
        }
        identity.retainedCanonicalIds.push_back(std::move(retainedId));
    }
    return true;
}

void writeEpoch(QDataStream& stream, const skygate::core::AstronomicalEpoch& epoch)
{
    stream << epoch.julianDatePart1 << epoch.julianDatePart2 << static_cast<std::uint8_t>(epoch.timeScale);
}

[[nodiscard]] bool readEpoch(QDataStream& stream, skygate::core::AstronomicalEpoch& epoch)
{
    std::uint8_t timeScale = 0U;
    stream >> epoch.julianDatePart1 >> epoch.julianDatePart2 >> timeScale;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    const auto parsedTimeScale = static_cast<skygate::core::TimeScale>(timeScale);
    if (!CatalogSnapshotValidator::isKnownTimeScale(parsedTimeScale)) {
        return false;
    }
    epoch.timeScale = parsedTimeScale;
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
    stream << astrometry.validityRange.has_value();
    if (astrometry.validityRange.has_value()) {
        writeDateRange(stream, *astrometry.validityRange);
    }
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
    bool hasValidityRange = false;
    stream >> hasValidityRange;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    if (!hasValidityRange) {
        astrometry.validityRange = std::nullopt;
        return true;
    }
    EphemerisDateRange range;
    if (!readDateRange(stream, range)) {
        return false;
    }
    astrometry.validityRange = std::move(range);
    return true;
}

void writeDeepSkyInfo(QDataStream& stream, const DeepSkyObjectInfo& info)
{
    stream << static_cast<std::uint8_t>(info.kind);
    stream << static_cast<std::uint64_t>(info.aliases.size());
    for (const std::string& alias : info.aliases) {
        writeString(stream, alias);
    }
    writeOptionalDouble(stream, info.majorAxisArcmin);
    writeOptionalDouble(stream, info.minorAxisArcmin);
    writeOptionalDouble(stream, info.positionAngleDeg);
}

[[nodiscard]] bool readDeepSkyInfo(QDataStream& stream, DeepSkyObjectInfo& info)
{
    std::uint8_t kind = 0U;
    std::uint64_t aliasCount = 0U;
    stream >> kind >> aliasCount;
    if (stream.status() != QDataStream::Ok || aliasCount > kMaxBodyCount
        || !CatalogSnapshotValidator::isKnownDeepSkyObjectKind(static_cast<DeepSkyObjectInfo::Kind>(kind))) {
        return false;
    }
    info.kind = static_cast<DeepSkyObjectInfo::Kind>(kind);
    info.aliases.clear();
    info.aliases.reserve(static_cast<std::size_t>(aliasCount));
    for (std::uint64_t index = 0; index < aliasCount; ++index) {
        std::string alias;
        if (!readString(stream, alias)) {
            return false;
        }
        info.aliases.push_back(std::move(alias));
    }
    return readOptionalDouble(stream, info.majorAxisArcmin) && readOptionalDouble(stream, info.minorAxisArcmin)
           && readOptionalDouble(stream, info.positionAngleDeg);
}

[[nodiscard]] bool readBodyCount(QDataStream& stream, std::uint64_t& count)
{
    stream >> count;
    return stream.status() == QDataStream::Ok && count <= kMaxBodyCount;
}

}  // namespace

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
        writeString(stream, body.id);
        writeString(stream, body.displayName);
        stream << static_cast<std::uint8_t>(body.kind) << body.visualMagnitude;
        writeObjectIdentity(stream, body.identity);
        stream << body.fixedEquatorial.has_value();
        if (body.fixedEquatorial.has_value()) {
            writeEquatorial(stream, *body.fixedEquatorial);
        }
        stream << body.starAstrometry.has_value();
        if (body.starAstrometry.has_value()) {
            writeStarAstrometry(stream, *body.starAstrometry);
        }
    }

    const auto distantBodies = catalog.distantBodies();
    stream << static_cast<std::uint64_t>(distantBodies.size());
    for (const DistantCelestialBody& body : distantBodies) {
        writeString(stream, body.id);
        writeString(stream, body.displayName);
        stream << static_cast<std::uint8_t>(body.kind) << body.visualMagnitude;
        writeObjectIdentity(stream, body.identity);
        stream << body.fixedEquatorial.has_value();
        if (body.fixedEquatorial.has_value()) {
            writeEquatorial(stream, *body.fixedEquatorial);
        }
        stream << body.deepSkyObject.has_value();
        if (body.deepSkyObject.has_value()) {
            writeDeepSkyInfo(stream, *body.deepSkyObject);
        }
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
    if (!readBodyCount(stream, ownGalaxyCount)) {
        return nullptr;
    }
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    ownGalaxyBodies.reserve(static_cast<std::size_t>(ownGalaxyCount));
    for (std::uint64_t index = 0; index < ownGalaxyCount; ++index) {
        OwnGalaxyCelestialBody body;
        if (!readString(stream, body.id) || !readString(stream, body.displayName)) {
            return nullptr;
        }
        std::uint8_t kind = 0U;
        stream >> kind >> body.visualMagnitude;
        if (stream.status() != QDataStream::Ok
            || !CatalogSnapshotValidator::isKnownBodyKind(static_cast<BaseCelestialBody::Kind>(kind))) {
            return nullptr;
        }
        body.kind = static_cast<BaseCelestialBody::Kind>(kind);
        if (!readObjectIdentity(stream, body.identity)) {
            return nullptr;
        }
        bool hasFixedEquatorial = false;
        stream >> hasFixedEquatorial;
        if (hasFixedEquatorial) {
            skygate::core::EquatorialCoordinate coordinate;
            if (!readEquatorial(stream, coordinate)) {
                return nullptr;
            }
            body.fixedEquatorial = coordinate;
        }
        bool hasStarAstrometry = false;
        stream >> hasStarAstrometry;
        if (stream.status() != QDataStream::Ok) {
            return nullptr;
        }
        if (hasStarAstrometry) {
            CatalogStarAstrometry astrometry;
            if (!readStarAstrometry(stream, astrometry)) {
                return nullptr;
            }
            body.starAstrometry = std::move(astrometry);
        }
        ownGalaxyBodies.push_back(std::move(body));
    }

    std::uint64_t distantCount = 0U;
    if (!readBodyCount(stream, distantCount)) {
        return nullptr;
    }
    std::vector<DistantCelestialBody> distantBodies;
    distantBodies.reserve(static_cast<std::size_t>(distantCount));
    for (std::uint64_t index = 0; index < distantCount; ++index) {
        DistantCelestialBody body;
        if (!readString(stream, body.id) || !readString(stream, body.displayName)) {
            return nullptr;
        }
        std::uint8_t kind = 0U;
        stream >> kind >> body.visualMagnitude;
        if (stream.status() != QDataStream::Ok
            || !CatalogSnapshotValidator::isKnownBodyKind(static_cast<BaseCelestialBody::Kind>(kind))) {
            return nullptr;
        }
        body.kind = static_cast<BaseCelestialBody::Kind>(kind);
        if (!readObjectIdentity(stream, body.identity)) {
            return nullptr;
        }
        bool hasFixedEquatorial = false;
        stream >> hasFixedEquatorial;
        if (hasFixedEquatorial) {
            skygate::core::EquatorialCoordinate coordinate;
            if (!readEquatorial(stream, coordinate)) {
                return nullptr;
            }
            body.fixedEquatorial = coordinate;
        }
        bool hasDeepSkyInfo = false;
        stream >> hasDeepSkyInfo;
        if (stream.status() != QDataStream::Ok) {
            return nullptr;
        }
        if (hasDeepSkyInfo) {
            DeepSkyObjectInfo info;
            if (!readDeepSkyInfo(stream, info)) {
                return nullptr;
            }
            body.deepSkyObject = std::move(info);
        }
        distantBodies.push_back(std::move(body));
    }

    std::uint64_t orderEntryCount = 0U;
    stream >> orderEntryCount;
    if (stream.status() != QDataStream::Ok || orderEntryCount > kMaxOrderEntryCount) {
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
