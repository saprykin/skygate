#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SkyCatalogBinaryCodec.hpp"
#include "catalog/InMemoryStarCatalog.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"

#include <QDataStream>
#include <QIODevice>
#include <QString>
#include <QtTest/QtTest>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

skygate::ephemeris::CelestialBodyCatalog makeTestCatalog()
{
    skygate::ephemeris::OwnGalaxyCelestialBody sirius;
    sirius.id = "hip_32349";
    sirius.displayName = "Sirius";
    sirius.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    sirius.visualMagnitude = -1.46;
    sirius.fixedEquatorial =
        skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.7525, .declinationDeg = -16.716};
    sirius.starAstrometry = skygate::ephemeris::CatalogStarAstrometry{
        .referenceEquatorial =
            skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.7525, .declinationDeg = -16.716},
        .referenceEpoch =
            skygate::core::AstronomicalEpoch{
                .julianDatePart1 = 2451545.0,
                .julianDatePart2 = 0.0,
                .timeScale = skygate::core::TimeScale::Tt,
            },
        .properMotionRightAscensionMasPerYear = -546.01,
        .properMotionDeclinationMasPerYear = -1223.07,
        .stellarParallaxMas = 379.21,
        .radialVelocityKmPerSecond = -5.5,
        .validityRange = skygate::ephemeris::EphemerisDateRange{
            .id = "range",
            .displayName = "Range",
            .start =
                skygate::core::AstronomicalEpoch{
                    .julianDatePart1 = 2451545.0,
                    .julianDatePart2 = -1000.0,
                    .timeScale = skygate::core::TimeScale::Tdb,
                },
            .end = skygate::core::AstronomicalEpoch{
                .julianDatePart1 = 2451545.0,
                .julianDatePart2 = 1000.0,
                .timeScale = skygate::core::TimeScale::Tdb,
            },
        },
    };

    skygate::ephemeris::OwnGalaxyCelestialBody anchor;
    anchor.id = "anchor_1";
    anchor.displayName = "Anchor";
    anchor.kind = skygate::ephemeris::BaseCelestialBody::Kind::Constellation;
    anchor.visualMagnitude = 2.0;
    anchor.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};

    skygate::ephemeris::DistantCelestialBody galaxy;
    galaxy.id = "ngc_224";
    galaxy.displayName = "Andromeda Galaxy";
    galaxy.kind = skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
    galaxy.visualMagnitude = 3.44;
    galaxy.fixedEquatorial =
        skygate::core::EquatorialCoordinate{.rightAscensionHours = 0.712, .declinationDeg = 41.269};
    galaxy.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{
        .kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = {"M31", "Messier 31"},
        .majorAxisArcmin = 190.5,
        .minorAxisArcmin = 61.7,
        .positionAngleDeg = 35.0,
    };

    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> order{
        {.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = 0U},
        {.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = 0U},
        {.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = 1U},
    };

    return skygate::ephemeris::CelestialBodyCatalog(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{sirius, anchor},
        std::vector<skygate::ephemeris::DistantCelestialBody>{galaxy},
        std::move(order)
    );
}

void verifyOwnGalaxyBody(
    const skygate::ephemeris::OwnGalaxyCelestialBody& actual, const skygate::ephemeris::OwnGalaxyCelestialBody& expected
)
{
    QCOMPARE(actual.id, expected.id);
    QCOMPARE(actual.displayName, expected.displayName);
    QCOMPARE(static_cast<std::uint8_t>(actual.kind), static_cast<std::uint8_t>(expected.kind));
    QCOMPARE(actual.visualMagnitude, expected.visualMagnitude);
    QCOMPARE(actual.fixedEquatorial.has_value(), expected.fixedEquatorial.has_value());
    if (actual.fixedEquatorial.has_value()) {
        QCOMPARE(actual.fixedEquatorial->rightAscensionHours, expected.fixedEquatorial->rightAscensionHours);
        QCOMPARE(actual.fixedEquatorial->declinationDeg, expected.fixedEquatorial->declinationDeg);
    }
    QCOMPARE(actual.starAstrometry.has_value(), expected.starAstrometry.has_value());
    if (actual.starAstrometry.has_value()) {
        const auto& actualAstrometry = *actual.starAstrometry;
        const auto& expectedAstrometry = *expected.starAstrometry;
        QCOMPARE(
            actualAstrometry.referenceEquatorial.rightAscensionHours,
            expectedAstrometry.referenceEquatorial.rightAscensionHours
        );
        QCOMPARE(
            actualAstrometry.referenceEquatorial.declinationDeg, expectedAstrometry.referenceEquatorial.declinationDeg
        );
        QCOMPARE(actualAstrometry.referenceEpoch.julianDatePart1, expectedAstrometry.referenceEpoch.julianDatePart1);
        QCOMPARE(actualAstrometry.referenceEpoch.julianDatePart2, expectedAstrometry.referenceEpoch.julianDatePart2);
        QCOMPARE(
            static_cast<std::uint8_t>(actualAstrometry.referenceEpoch.timeScale),
            static_cast<std::uint8_t>(expectedAstrometry.referenceEpoch.timeScale)
        );
        QCOMPARE(
            actualAstrometry.properMotionRightAscensionMasPerYear,
            expectedAstrometry.properMotionRightAscensionMasPerYear
        );
        QCOMPARE(
            actualAstrometry.properMotionDeclinationMasPerYear, expectedAstrometry.properMotionDeclinationMasPerYear
        );
        QCOMPARE(actualAstrometry.stellarParallaxMas, expectedAstrometry.stellarParallaxMas);
        QCOMPARE(actualAstrometry.radialVelocityKmPerSecond, expectedAstrometry.radialVelocityKmPerSecond);
        QCOMPARE(actualAstrometry.validityRange.has_value(), expectedAstrometry.validityRange.has_value());
        if (actualAstrometry.validityRange.has_value()) {
            QCOMPARE(actualAstrometry.validityRange->id, expectedAstrometry.validityRange->id);
            QCOMPARE(actualAstrometry.validityRange->displayName, expectedAstrometry.validityRange->displayName);
            QCOMPARE(
                actualAstrometry.validityRange->start.julianDatePart1,
                expectedAstrometry.validityRange->start.julianDatePart1
            );
            QCOMPARE(
                actualAstrometry.validityRange->start.julianDatePart2,
                expectedAstrometry.validityRange->start.julianDatePart2
            );
            QCOMPARE(
                static_cast<std::uint8_t>(actualAstrometry.validityRange->start.timeScale),
                static_cast<std::uint8_t>(expectedAstrometry.validityRange->start.timeScale)
            );
        }
    }
}

void verifyDistantBody(
    const skygate::ephemeris::DistantCelestialBody& actual, const skygate::ephemeris::DistantCelestialBody& expected
)
{
    QCOMPARE(actual.id, expected.id);
    QCOMPARE(actual.displayName, expected.displayName);
    QCOMPARE(static_cast<std::uint8_t>(actual.kind), static_cast<std::uint8_t>(expected.kind));
    QCOMPARE(actual.visualMagnitude, expected.visualMagnitude);
    QCOMPARE(actual.fixedEquatorial.has_value(), expected.fixedEquatorial.has_value());
    if (actual.fixedEquatorial.has_value()) {
        QCOMPARE(actual.fixedEquatorial->rightAscensionHours, expected.fixedEquatorial->rightAscensionHours);
        QCOMPARE(actual.fixedEquatorial->declinationDeg, expected.fixedEquatorial->declinationDeg);
    }
    QCOMPARE(actual.deepSkyObject.has_value(), expected.deepSkyObject.has_value());
    if (actual.deepSkyObject.has_value()) {
        QCOMPARE(
            static_cast<std::uint8_t>(actual.deepSkyObject->kind),
            static_cast<std::uint8_t>(expected.deepSkyObject->kind)
        );
        QCOMPARE(actual.deepSkyObject->aliases, expected.deepSkyObject->aliases);
        QCOMPARE(actual.deepSkyObject->majorAxisArcmin, expected.deepSkyObject->majorAxisArcmin);
        QCOMPARE(actual.deepSkyObject->minorAxisArcmin, expected.deepSkyObject->minorAxisArcmin);
        QCOMPARE(actual.deepSkyObject->positionAngleDeg, expected.deepSkyObject->positionAngleDeg);
    }
}

constexpr std::uint32_t kTestBinaryCatalogMagic = 0x53474243U;  // "SGBC"

void writePayloadString(QDataStream& stream, const std::string& value)
{
    stream << QString::fromStdString(value);
}

void writeBinaryHeader(QDataStream& stream)
{
    stream << kTestBinaryCatalogMagic << skygate::ui::internal::SkyCatalogBinaryCodec::kSchemaVersion;
}

void writeOwnGalaxyBodyWithKind(QDataStream& stream, const std::uint8_t kind)
{
    writePayloadString(stream, "hip_test");
    writePayloadString(stream, "Test");
    stream << kind << 0.0 << false << false;
}

void writeDistantBodyWithKind(QDataStream& stream, const std::uint8_t kind)
{
    writePayloadString(stream, "ngc_test");
    writePayloadString(stream, "Test");
    stream << kind << 0.0 << false << false;
}

void writeDistantBodyWithDeepSkyInfoKind(QDataStream& stream, const std::uint8_t infoKind)
{
    writePayloadString(stream, "ngc_test");
    writePayloadString(stream, "Test");
    stream << static_cast<std::uint8_t>(skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject);
    stream << 0.0;
    stream << false;
    stream << true;
    stream << infoKind;
    stream << static_cast<std::uint64_t>(0U);
    stream << false << false << false;
}

void writeStarBodyWithTimeScale(QDataStream& stream, const std::uint8_t timeScale)
{
    writePayloadString(stream, "hip_test");
    writePayloadString(stream, "Test");
    stream << static_cast<std::uint8_t>(skygate::ephemeris::BaseCelestialBody::Kind::Star);
    stream << 0.0;
    stream << false;
    stream << true;
    stream << 0.0 << 0.0;
    stream << 0.0 << 0.0 << timeScale;
    stream << false << false << false << false << false;
}

QByteArray makeOrderEntryPayload(const std::uint8_t domain, const std::uint64_t bodyIndex)
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream.setByteOrder(QDataStream::LittleEndian);
    writeBinaryHeader(stream);
    stream << static_cast<std::uint64_t>(0U);
    stream << static_cast<std::uint64_t>(0U);
    stream << static_cast<std::uint64_t>(1U);
    stream << domain << bodyIndex;
    return buffer;
}

QByteArray
makeSingleBodyOrderPayload(const std::uint8_t domain, const std::uint64_t bodyIndex, const bool ownGalaxyBody)
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream.setByteOrder(QDataStream::LittleEndian);
    writeBinaryHeader(stream);
    if (ownGalaxyBody) {
        stream << static_cast<std::uint64_t>(1U);
        writeOwnGalaxyBodyWithKind(
            stream, static_cast<std::uint8_t>(skygate::ephemeris::BaseCelestialBody::Kind::Star)
        );
        stream << static_cast<std::uint64_t>(0U);
    } else {
        stream << static_cast<std::uint64_t>(0U);
        stream << static_cast<std::uint64_t>(1U);
        writeDistantBodyWithKind(
            stream, static_cast<std::uint8_t>(skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject)
        );
    }
    stream << static_cast<std::uint64_t>(1U);
    stream << domain << bodyIndex;
    return buffer;
}

QByteArray makeInvalidOwnGalaxyKindPayload(const std::uint8_t kind)
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream.setByteOrder(QDataStream::LittleEndian);
    writeBinaryHeader(stream);
    stream << static_cast<std::uint64_t>(1U);
    writeOwnGalaxyBodyWithKind(stream, kind);
    stream << static_cast<std::uint64_t>(0U);
    stream << static_cast<std::uint64_t>(0U);
    return buffer;
}

QByteArray makeInvalidDeepSkyKindPayload(const std::uint8_t kind)
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream.setByteOrder(QDataStream::LittleEndian);
    writeBinaryHeader(stream);
    stream << static_cast<std::uint64_t>(0U);
    stream << static_cast<std::uint64_t>(1U);
    writeDistantBodyWithDeepSkyInfoKind(stream, kind);
    stream << static_cast<std::uint64_t>(0U);
    return buffer;
}

QByteArray makeInvalidTimeScalePayload(const std::uint8_t timeScale)
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream.setByteOrder(QDataStream::LittleEndian);
    writeBinaryHeader(stream);
    stream << static_cast<std::uint64_t>(1U);
    writeStarBodyWithTimeScale(stream, timeScale);
    stream << static_cast<std::uint64_t>(0U);
    stream << static_cast<std::uint64_t>(0U);
    return buffer;
}

}  // namespace

class SkyCatalogBinaryCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void roundTripsCatalogContents();
    void rejectsEmptyPayload();
    void rejectsCorruptPayload();
    void rejectsUnknownOrderDomainWithEmptyVectors();
    void rejectsUnknownOrderDomainWithExtremeIndex();
    void rejectsInvalidOwnGalaxyBodyKind();
    void rejectsInvalidDeepSkyObjectKind();
    void rejectsInvalidTimeScale();
    void rejectsKnownDomainIndexOutOfRange();
};

void SkyCatalogBinaryCodecTests::roundTripsCatalogContents()
{
    const skygate::ephemeris::CelestialBodyCatalog source = makeTestCatalog();
    const QByteArray payload = skygate::ui::internal::SkyCatalogBinaryCodec::serialize(source);
    QVERIFY(!payload.isEmpty());

    const std::unique_ptr<skygate::ephemeris::IStarCatalog> restored =
        skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(payload);
    QVERIFY(restored != nullptr);
    QCOMPARE(restored->catalog().ownGalaxyBodies().size(), source.ownGalaxyBodies().size());
    QCOMPARE(restored->catalog().distantBodies().size(), source.distantBodies().size());
    QCOMPARE(restored->catalog().orderedBodyIndexes().size(), source.orderedBodyIndexes().size());
    QCOMPARE(restored->bodies().size(), source.bodies().size());

    const auto actualOwnGalaxy = restored->catalog().ownGalaxyBodies();
    const auto expectedOwnGalaxy = source.ownGalaxyBodies();
    for (std::size_t index = 0; index < actualOwnGalaxy.size(); ++index) {
        verifyOwnGalaxyBody(actualOwnGalaxy[index], expectedOwnGalaxy[index]);
    }

    const auto actualDistant = restored->catalog().distantBodies();
    const auto expectedDistant = source.distantBodies();
    for (std::size_t index = 0; index < actualDistant.size(); ++index) {
        verifyDistantBody(actualDistant[index], expectedDistant[index]);
    }

    const auto actualOrder = restored->catalog().orderedBodyIndexes();
    const auto expectedOrder = source.orderedBodyIndexes();
    for (std::size_t index = 0; index < actualOrder.size(); ++index) {
        QCOMPARE(
            static_cast<std::uint8_t>(actualOrder[index].domain), static_cast<std::uint8_t>(expectedOrder[index].domain)
        );
        QCOMPARE(actualOrder[index].bodyIndex, expectedOrder[index].bodyIndex);
    }

    // Restored bodies follow the original order.
    const auto actualBodies = restored->bodies();
    const auto expectedBodies = source.bodies();
    for (std::size_t index = 0; index < actualBodies.size(); ++index) {
        QCOMPARE(std::string{actualBodies[index]->id}, std::string{expectedBodies[index]->id});
    }
}

void SkyCatalogBinaryCodecTests::rejectsEmptyPayload()
{
    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(QByteArray{}) == nullptr);
}

void SkyCatalogBinaryCodecTests::rejectsCorruptPayload()
{
    const skygate::ephemeris::CelestialBodyCatalog source = makeTestCatalog();
    QByteArray payload = skygate::ui::internal::SkyCatalogBinaryCodec::serialize(source);

    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(payload.mid(0, payload.size() / 2)) == nullptr);
    payload[4] = static_cast<char>(payload[4] ^ 0xFF);
    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(payload) == nullptr);
}

void SkyCatalogBinaryCodecTests::rejectsUnknownOrderDomainWithEmptyVectors()
{
    const QByteArray payload = makeOrderEntryPayload(0xFFU, 0U);
    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(payload) == nullptr);
}

void SkyCatalogBinaryCodecTests::rejectsUnknownOrderDomainWithExtremeIndex()
{
    const QByteArray payload = makeOrderEntryPayload(0xFFU, std::numeric_limits<std::uint64_t>::max());
    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(payload) == nullptr);
}

void SkyCatalogBinaryCodecTests::rejectsInvalidOwnGalaxyBodyKind()
{
    const QByteArray payload = makeInvalidOwnGalaxyKindPayload(0xFFU);
    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(payload) == nullptr);
}

void SkyCatalogBinaryCodecTests::rejectsInvalidDeepSkyObjectKind()
{
    const QByteArray payload = makeInvalidDeepSkyKindPayload(0xFFU);
    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(payload) == nullptr);
}

void SkyCatalogBinaryCodecTests::rejectsInvalidTimeScale()
{
    const QByteArray payload = makeInvalidTimeScalePayload(0xFFU);
    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(payload) == nullptr);
}

void SkyCatalogBinaryCodecTests::rejectsKnownDomainIndexOutOfRange()
{
    const auto ownGalaxyDomain =
        static_cast<std::uint8_t>(skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy);
    const auto distantDomain = static_cast<std::uint8_t>(skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant);

    const QByteArray ownGalaxyPayload = makeSingleBodyOrderPayload(ownGalaxyDomain, 1U, true);
    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(ownGalaxyPayload) == nullptr);

    const QByteArray distantPayload = makeSingleBodyOrderPayload(distantDomain, 1U, false);
    QVERIFY(skygate::ui::internal::SkyCatalogBinaryCodec::deserialize(distantPayload) == nullptr);
}

QTEST_GUILESS_MAIN(SkyCatalogBinaryCodecTests)

#include "SkyCatalogBinaryCodecTests.moc"
