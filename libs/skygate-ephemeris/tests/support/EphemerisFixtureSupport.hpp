#pragma once

#include <QByteArray>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace skygate::ephemeris::tests {

struct EphemerisFixtureMetadata {
    QString source;
    QString sourceUrl;
    QString apiParameters;
    QString generatedDate;
    QString sourceFrame;
    QString timeScale;
    QString target;
    QString observer;
};

struct EphemerisRaDecExpectation {
    double rightAscensionHours = 0.0;
    double declinationDegrees = 0.0;
};

struct EphemerisRaDecFixture {
    EphemerisFixtureMetadata metadata;
    EphemerisRaDecExpectation expected;
    double toleranceDegrees = 0.0;
};

[[nodiscard]] inline double rightAscensionHoursToDegrees(const double rightAscensionHours) noexcept
{
    return rightAscensionHours * 15.0;
}

[[nodiscard]] inline bool hasFiniteCoordinates(const EphemerisRaDecExpectation& expectation) noexcept
{
    return std::isfinite(expectation.rightAscensionHours) && std::isfinite(expectation.declinationDegrees);
}

[[nodiscard]] inline double
angularSeparationDegrees(const EphemerisRaDecExpectation& lhs, const EphemerisRaDecExpectation& rhs) noexcept
{
    if (!hasFiniteCoordinates(lhs) || !hasFiniteCoordinates(rhs)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    constexpr double kDegreesToRadians = 3.141592653589793238462643383279502884 / 180.0;
    constexpr double kRadiansToDegrees = 180.0 / 3.141592653589793238462643383279502884;

    const double lhsRa = rightAscensionHoursToDegrees(lhs.rightAscensionHours) * kDegreesToRadians;
    const double rhsRa = rightAscensionHoursToDegrees(rhs.rightAscensionHours) * kDegreesToRadians;
    const double lhsDec = lhs.declinationDegrees * kDegreesToRadians;
    const double rhsDec = rhs.declinationDegrees * kDegreesToRadians;

    const double sinHalfDec = std::sin((rhsDec - lhsDec) / 2.0);
    const double sinHalfRa = std::sin((rhsRa - lhsRa) / 2.0);
    const double haversine = sinHalfDec * sinHalfDec + std::cos(lhsDec) * std::cos(rhsDec) * sinHalfRa * sinHalfRa;
    return 2.0 * std::asin(std::min(1.0, std::sqrt(std::max(0.0, haversine)))) * kRadiansToDegrees;
}

[[nodiscard]] inline bool isWithinAngularTolerance(
    const EphemerisRaDecExpectation& actual, const EphemerisRaDecExpectation& expected, const double toleranceDegrees
) noexcept
{
    if (!hasFiniteCoordinates(actual) || !hasFiniteCoordinates(expected) || !std::isfinite(toleranceDegrees)
        || toleranceDegrees < 0.0) {
        return false;
    }

    return angularSeparationDegrees(actual, expected) <= toleranceDegrees;
}

[[nodiscard]] inline bool isGitLfsPointerPayload(const QByteArray& payload)
{
    return payload.startsWith("version https://git-lfs.github.com/spec/v1\n");
}

[[nodiscard]] inline std::optional<EphemerisRaDecFixture>
loadRaDecFixture(const QString& path, QString* errorText = nullptr)
{
    auto fail = [errorText](const QString& message) -> std::optional<EphemerisRaDecFixture> {
        if (errorText != nullptr) {
            *errorText = message;
        }
        return std::nullopt;
    };

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("Could not open fixture"));
    }

    const QByteArray payload = file.readAll();
    if (isGitLfsPointerPayload(payload)) {
        return fail(QStringLiteral("Fixture is a Git LFS pointer, not fixture content"));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QStringLiteral("Fixture is not valid JSON object content"));
    }

    const QJsonObject root = document.object();
    const QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
    const QJsonObject expected = root.value(QStringLiteral("expected")).toObject();
    const QJsonObject tolerance = root.value(QStringLiteral("tolerance")).toObject();

    const QStringList requiredMetadata{
        QStringLiteral("source"),
        QStringLiteral("sourceUrl"),
        QStringLiteral("apiParameters"),
        QStringLiteral("generatedDate"),
        QStringLiteral("sourceFrame"),
        QStringLiteral("timeScale"),
        QStringLiteral("target"),
        QStringLiteral("observer"),
    };
    for (const QString& key : requiredMetadata) {
        if (!metadata.value(key).isString() || metadata.value(key).toString().trimmed().isEmpty()) {
            return fail(QStringLiteral("Fixture metadata field is missing or empty: %1").arg(key));
        }
    }

    if (!expected.value(QStringLiteral("rightAscensionHours")).isDouble()
        || !expected.value(QStringLiteral("declinationDegrees")).isDouble()) {
        return fail(QStringLiteral("Fixture expected RA/Dec fields are missing or non-numeric"));
    }
    if (!tolerance.value(QStringLiteral("angularDegrees")).isDouble()) {
        return fail(QStringLiteral("Fixture angular tolerance is missing or non-numeric"));
    }

    EphemerisRaDecFixture fixture;
    fixture.metadata = {
        .source = metadata.value(QStringLiteral("source")).toString(),
        .sourceUrl = metadata.value(QStringLiteral("sourceUrl")).toString(),
        .apiParameters = metadata.value(QStringLiteral("apiParameters")).toString(),
        .generatedDate = metadata.value(QStringLiteral("generatedDate")).toString(),
        .sourceFrame = metadata.value(QStringLiteral("sourceFrame")).toString(),
        .timeScale = metadata.value(QStringLiteral("timeScale")).toString(),
        .target = metadata.value(QStringLiteral("target")).toString(),
        .observer = metadata.value(QStringLiteral("observer")).toString(),
    };
    fixture.expected = {
        .rightAscensionHours = expected.value(QStringLiteral("rightAscensionHours")).toDouble(),
        .declinationDegrees = expected.value(QStringLiteral("declinationDegrees")).toDouble(),
    };
    fixture.toleranceDegrees = tolerance.value(QStringLiteral("angularDegrees")).toDouble();

    if (!std::isfinite(fixture.expected.rightAscensionHours) || !std::isfinite(fixture.expected.declinationDegrees)
        || !std::isfinite(fixture.toleranceDegrees) || fixture.toleranceDegrees <= 0.0) {
        return fail(QStringLiteral("Fixture numeric fields must be finite and tolerance must be positive"));
    }

    return fixture;
}

}  // namespace skygate::ephemeris::tests
