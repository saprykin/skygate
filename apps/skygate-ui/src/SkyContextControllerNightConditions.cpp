#include "SkyContextController.hpp"

#include "SkyContextControllerSupport.hpp"
#include "skygate/ephemeris/NightConditionsCalculator.hpp"

#include <QDateTime>
#include <QVariantList>

#include <cmath>
#include <optional>
#include <span>
#include <string_view>

namespace {

[[nodiscard]] std::optional<std::uint32_t> bodyIndexById(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const std::string_view bodyId
)
{
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index].id == bodyId) {
            return static_cast<std::uint32_t>(index);
        }
    }
    return std::nullopt;
}

[[nodiscard]] QString formatEventValue(
    const skygate::ephemeris::ObservationEvent& event
)
{
    using skygate::ephemeris::ObservationEventStatus;

    switch (event.status) {
    case ObservationEventStatus::Available:
        if (event.utcTime.has_value()) {
            return skygate::ui::internal::SkyContextTimeCodec::toQDateTimeUtc(
                *event.utcTime
            ).toString("HH:mm");
        }
        return "--";
    case ObservationEventStatus::AlwaysAbove:
        return "Always up";
    case ObservationEventStatus::AlwaysBelow:
        return "Always down";
    case ObservationEventStatus::NoEventInSearchWindow:
        return "No event";
    case ObservationEventStatus::InvalidInput:
    case ObservationEventStatus::Unresolved:
        return "--";
    }

    return "--";
}

[[nodiscard]] QVariantMap eventRow(
    const QString& label,
    const skygate::ephemeris::ObservationEvent& event
)
{
    return QVariantMap {
        {QStringLiteral("label"), label},
        {QStringLiteral("value"), formatEventValue(event)}
    };
}

[[nodiscard]] QVariantMap placeholderConditionsMap(
    const skygate::core::SkyContext& context
)
{
    return QVariantMap {
        {QStringLiteral("valid"), false},
        {
            QStringLiteral("locationText"),
            QString("UTC | Lat %1 Lon %2")
                .arg(skygate::ui::internal::SkyContextTextFormatter::formatCoordinate(
                    context.observer.latitudeDeg
                ))
                .arg(skygate::ui::internal::SkyContextTextFormatter::formatCoordinate(
                    context.observer.longitudeDeg
                ))
        },
        {QStringLiteral("sunRows"), QVariantList {}},
        {QStringLiteral("moonPhaseText"), QStringLiteral("--")},
        {QStringLiteral("moonRiseText"), QStringLiteral("--")},
        {QStringLiteral("moonSetText"), QStringLiteral("--")}
    };
}

[[nodiscard]] QVariantMap conditionsMap(
    const skygate::core::SkyContext& context,
    const skygate::ephemeris::NightConditions& conditions
)
{
    if (!conditions.valid) {
        return placeholderConditionsMap(context);
    }

    QVariantList sunRows;
    sunRows.push_back(eventRow(QStringLiteral("Sunset"), conditions.sunset));
    sunRows.push_back(eventRow(QStringLiteral("Civil dusk"), conditions.civilDusk));
    sunRows.push_back(eventRow(QStringLiteral("Nautical dusk"), conditions.nauticalDusk));
    sunRows.push_back(eventRow(QStringLiteral("Astro dusk"), conditions.astronomicalDusk));
    sunRows.push_back(eventRow(QStringLiteral("Astro dawn"), conditions.astronomicalDawn));
    sunRows.push_back(eventRow(QStringLiteral("Sunrise"), conditions.sunrise));

    return QVariantMap {
        {QStringLiteral("valid"), true},
        {
            QStringLiteral("locationText"),
            QString("UTC | Lat %1 Lon %2")
                .arg(skygate::ui::internal::SkyContextTextFormatter::formatCoordinate(
                    context.observer.latitudeDeg
                ))
                .arg(skygate::ui::internal::SkyContextTextFormatter::formatCoordinate(
                    context.observer.longitudeDeg
                ))
        },
        {QStringLiteral("sunRows"), sunRows},
        {
            QStringLiteral("moonPhaseText"),
            QString("%1 | %2%")
                .arg(QString::fromStdString(conditions.moonPhaseName))
                .arg(qRound(conditions.moonIlluminationPercent))
        },
        {QStringLiteral("moonRiseText"), formatEventValue(conditions.moonrise)},
        {QStringLiteral("moonSetText"), formatEventValue(conditions.moonset)}
    };
}

}  // namespace

QString SkyContextController::nightConditionsIconKind() const
{
    const auto* engine = ephemerisEngine();
    if (engine == nullptr || !m_location.context.observer.isValid()) {
        return QStringLiteral("unknown");
    }

    const auto sunState = engine->computeBodyState(m_location.context, std::string_view("sun"));
    if (!sunState.has_value() || !sunState->horizontal.isFinite()) {
        return QStringLiteral("unknown");
    }

    if (sunState->horizontal.altitudeDeg > 0.0) {
        return QStringLiteral("sun");
    }
    if (sunState->horizontal.altitudeDeg > -18.0) {
        return QStringLiteral("twilight");
    }
    return QStringLiteral("moon");
}

void SkyContextController::refreshNightConditions()
{
    const auto bodies = catalogBodies();
    const auto sunIndex = bodyIndexById(bodies, "sun");
    const auto moonIndex = bodyIndexById(bodies, "moon");
    const auto* engine = ephemerisEngine();

    QVariantMap nextConditions = placeholderConditionsMap(m_location.context);
    if (engine != nullptr && sunIndex.has_value() && moonIndex.has_value()) {
        const skygate::ephemeris::NightConditionsCalculator calculator;
        nextConditions = conditionsMap(
            m_location.context,
            calculator.compute(*engine, m_location.context, *sunIndex, *moonIndex)
        );
    }

    if (m_nightConditions == nextConditions) {
        emit nightConditionsChanged();
        return;
    }

    m_nightConditions = nextConditions;
    emit nightConditionsChanged();
}
