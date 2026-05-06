#include "SkyContextController.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkyTimeController.hpp"
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

class NightConditionsMapBuilder final {
public:
    NightConditionsMapBuilder(
        const skygate::core::SkyContext& context,
        const SkyTimeController* timeController
    )
        : m_context(context)
        , m_timeController(timeController)
    {
    }

    [[nodiscard]] QVariantMap placeholderMap() const
    {
        return QVariantMap {
            {QStringLiteral("valid"), false},
            {QStringLiteral("locationText"), locationText()},
            {QStringLiteral("sunRows"), QVariantList {}},
            {QStringLiteral("moonPhaseText"), QStringLiteral("--")},
            {QStringLiteral("moonRiseText"), QStringLiteral("--")},
            {QStringLiteral("moonSetText"), QStringLiteral("--")}
        };
    }

    [[nodiscard]] QVariantMap conditionsMap(
        const skygate::ephemeris::NightConditions& conditions
    ) const
    {
        if (!conditions.valid) {
            return placeholderMap();
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
            {QStringLiteral("locationText"), locationText()},
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

private:
    [[nodiscard]] QString formatEventValue(
        const skygate::ephemeris::ObservationEvent& event
    ) const
    {
        using skygate::ephemeris::ObservationEventStatus;

        switch (event.status) {
        case ObservationEventStatus::Available:
            if (event.utcTime.has_value()) {
                return m_timeController != nullptr
                    ? m_timeController->formatUtcTime(*event.utcTime, false)
                    : skygate::ui::internal::SkyContextTimeCodec::toQDateTimeUtc(
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
    ) const
    {
        return QVariantMap {
            {QStringLiteral("label"), label},
            {QStringLiteral("value"), formatEventValue(event)}
        };
    }

    [[nodiscard]] QString locationText() const
    {
        return QString("%1 | Lat %2 Lon %3")
            .arg(m_timeController != nullptr
                ? m_timeController->timeZoneLabel()
                : QStringLiteral("UTC"))
            .arg(skygate::ui::internal::SkyContextTextFormatter::formatCoordinate(
                m_context.observer.latitudeDeg
            ))
            .arg(skygate::ui::internal::SkyContextTextFormatter::formatCoordinate(
                m_context.observer.longitudeDeg
            ));
    }

private:
    const skygate::core::SkyContext& m_context;
    const SkyTimeController* m_timeController = nullptr;
};

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
    const NightConditionsMapBuilder mapBuilder(m_location.context, m_timeController.get());

    QVariantMap nextConditions = mapBuilder.placeholderMap();
    if (engine != nullptr && sunIndex.has_value() && moonIndex.has_value()) {
        const skygate::ephemeris::NightConditionsCalculator calculator;
        nextConditions = mapBuilder.conditionsMap(
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
