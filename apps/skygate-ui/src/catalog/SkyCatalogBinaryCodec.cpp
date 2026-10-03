#include "SkyCatalogBinaryCodec.hpp"

#include "CelestialBodyCatalog.hpp"
#include "EquatorialCoordinate.hpp"
#include "catalog/InMemoryStarCatalog.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"

#include <QDataStream>
#include <QIODevice>
#include <QString>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace skygate::ui::internal {
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
    epoch.timeScale = static_cast<skygate::core::TimeScale>(timeScale);
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

void writeDateRange(QDataStream& stream, const skygate::ephemeris::EphemerisDateRange& range)
{
    writeString(stream, range.id);
    writeString(stream, range.displayName);
    writeEpoch(stream, range.start);
    writeEpoch(stream, range.end);
}

[[nodiscard]] bool readDateRange(QDataStream& stream, skygate::ephemeris::EphemerisDateRange& range)
{
    return readString(stream, range.id) && readString(stream, range.displayName) && readEpoch(stream, range.start)
           && readEpoch(stream, range.end);
}

void writeStarAstrometry(QDataStream& stream, const skygate::ephemeris::CatalogStarAstrometry& astrometry)
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

[[nodiscard]] bool readStarAstrometry(QDataStream& stream, skygate::ephemeris::CatalogStarAstrometry& astrometry)
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
    skygate::ephemeris::EphemerisDateRange range;
    if (!readDateRange(stream, range)) {
        return false;
    }
    astrometry.validityRange = std::move(range);
    return true;
}

void writeDeepSkyInfo(QDataStream& stream, const skygate::ephemeris::DeepSkyObjectInfo& info)
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

[[nodiscard]] bool readDeepSkyInfo(QDataStream& stream, skygate::ephemeris::DeepSkyObjectInfo& info)
{
    std::uint8_t kind = 0U;
    std::uint64_t aliasCount = 0U;
    stream >> kind >> aliasCount;
    if (stream.status() != QDataStream::Ok || aliasCount > kMaxBodyCount) {
        return false;
    }
    info.kind = static_cast<skygate::ephemeris::DeepSkyObjectInfo::Kind>(kind);
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

QByteArray SkyCatalogBinaryCodec::serialize(const skygate::ephemeris::CelestialBodyCatalog& catalog)
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << kBinaryCatalogMagic << kSchemaVersion;

    const auto ownGalaxyBodies = catalog.ownGalaxyBodies();
    stream << static_cast<std::uint64_t>(ownGalaxyBodies.size());
    for (const skygate::ephemeris::OwnGalaxyCelestialBody& body : ownGalaxyBodies) {
        writeString(stream, body.id);
        writeString(stream, body.displayName);
        stream << static_cast<std::uint8_t>(body.kind) << body.visualMagnitude;
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
    for (const skygate::ephemeris::DistantCelestialBody& body : distantBodies) {
        writeString(stream, body.id);
        writeString(stream, body.displayName);
        stream << static_cast<std::uint8_t>(body.kind) << body.visualMagnitude;
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
    for (const skygate::ephemeris::CelestialBodyCatalog::OrderEntry& entry : orderedIndexes) {
        stream << static_cast<std::uint8_t>(entry.domain) << static_cast<std::uint64_t>(entry.bodyIndex);
    }

    return buffer;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> SkyCatalogBinaryCodec::deserialize(const QByteArray& payload)
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
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> ownGalaxyBodies;
    ownGalaxyBodies.reserve(static_cast<std::size_t>(ownGalaxyCount));
    for (std::uint64_t index = 0; index < ownGalaxyCount; ++index) {
        skygate::ephemeris::OwnGalaxyCelestialBody body;
        if (!readString(stream, body.id) || !readString(stream, body.displayName)) {
            return nullptr;
        }
        std::uint8_t kind = 0U;
        stream >> kind >> body.visualMagnitude;
        if (stream.status() != QDataStream::Ok) {
            return nullptr;
        }
        body.kind = static_cast<skygate::ephemeris::BaseCelestialBody::Kind>(kind);
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
            skygate::ephemeris::CatalogStarAstrometry astrometry;
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
    std::vector<skygate::ephemeris::DistantCelestialBody> distantBodies;
    distantBodies.reserve(static_cast<std::size_t>(distantCount));
    for (std::uint64_t index = 0; index < distantCount; ++index) {
        skygate::ephemeris::DistantCelestialBody body;
        if (!readString(stream, body.id) || !readString(stream, body.displayName)) {
            return nullptr;
        }
        std::uint8_t kind = 0U;
        stream >> kind >> body.visualMagnitude;
        if (stream.status() != QDataStream::Ok) {
            return nullptr;
        }
        body.kind = static_cast<skygate::ephemeris::BaseCelestialBody::Kind>(kind);
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
            skygate::ephemeris::DeepSkyObjectInfo info;
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
    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> orderedIndexes;
    orderedIndexes.reserve(static_cast<std::size_t>(orderEntryCount));
    for (std::uint64_t index = 0; index < orderEntryCount; ++index) {
        std::uint8_t domain = 0U;
        std::uint64_t bodyIndex = 0U;
        stream >> domain >> bodyIndex;
        if (stream.status() != QDataStream::Ok) {
            return nullptr;
        }
        const bool indexOutOfRange =
            (domain == static_cast<std::uint8_t>(skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant)
             && bodyIndex >= distantBodies.size())
            || (domain == static_cast<std::uint8_t>(skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy)
                && bodyIndex >= ownGalaxyBodies.size());
        if (indexOutOfRange) {
            return nullptr;
        }
        orderedIndexes.push_back({
            .domain = static_cast<skygate::ephemeris::CelestialBodyCatalog::BodyDomain>(domain),
            .bodyIndex = static_cast<std::size_t>(bodyIndex),
        });
    }
    if (!stream.atEnd()) {
        return nullptr;
    }

    return std::make_unique<skygate::ephemeris::InMemoryStarCatalog>(skygate::ephemeris::CelestialBodyCatalog(
        std::move(ownGalaxyBodies), std::move(distantBodies), std::move(orderedIndexes)
    ));
}

}  // namespace skygate::ui::internal
