#include "SkyContextController.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkyNightConditionsAdapter.hpp"
#include "SkyTimeController.hpp"
#include "skygate/ephemeris/NightConditionsCalculator.hpp"

#include <QDateTime>

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

class NightConditionsDataBuilder final {
public:
    NightConditionsDataBuilder(
        const skygate::core::SkyContext& context,
        const SkyTimeController* timeController
    )
        : m_context(context)
        , m_timeController(timeController)
    {
    }

    [[nodiscard]] SkyNightConditionsData placeholderData() const
    {
        return SkyNightConditionsData {
            .valid = false,
            .locationText = locationText(),
            .sunRows = {},
            .moonPhaseText = QStringLiteral("--"),
            .moonRiseText = QStringLiteral("--"),
            .moonSetText = QStringLiteral("--")
        };
    }

    [[nodiscard]] SkyNightConditionsData conditionsData(
        const skygate::ephemeris::NightConditions& conditions
    ) const
    {
        if (!conditions.valid) {
            return placeholderData();
        }

        return SkyNightConditionsData {
            .valid = true,
            .locationText = locationText(),
            .sunRows = {
                eventRow(QStringLiteral("Sunset"), conditions.sunset),
                eventRow(QStringLiteral("Civil dusk"), conditions.civilDusk),
                eventRow(QStringLiteral("Nautical dusk"), conditions.nauticalDusk),
                eventRow(QStringLiteral("Astro dusk"), conditions.astronomicalDusk),
                eventRow(QStringLiteral("Astro dawn"), conditions.astronomicalDawn),
                eventRow(QStringLiteral("Sunrise"), conditions.sunrise)
            },
            .moonPhaseText = QString("%1 | %2%")
                .arg(QString::fromStdString(conditions.moonPhaseName))
                .arg(qRound(conditions.moonIlluminationPercent)),
            .moonRiseText = formatEventValue(conditions.moonrise),
            .moonSetText = formatEventValue(conditions.moonset)
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

    [[nodiscard]] SkyNightConditionSunRow eventRow(
        const QString& label,
        const skygate::ephemeris::ObservationEvent& event
    ) const
    {
        return SkyNightConditionSunRow {
            .label = label,
            .value = formatEventValue(event)
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
    if (engine == nullptr || !m_location.context().observer.isValid()) {
        return QStringLiteral("unknown");
    }

    const auto sunState = engine->computeBodyState(m_location.context(), std::string_view("sun"));
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
    const NightConditionsDataBuilder dataBuilder(m_location.context(), m_timeController.get());
    const SkyNightConditionsAdapter adapter;

    SkyNightConditionsData nextData = dataBuilder.placeholderData();
    if (engine != nullptr && sunIndex.has_value() && moonIndex.has_value()) {
        const skygate::ephemeris::NightConditionsCalculator calculator;
        nextData = dataBuilder.conditionsData(
            calculator.compute(*engine, m_location.context(), *sunIndex, *moonIndex)
        );
    }
    const QVariantMap nextConditions = adapter.nightConditions(nextData);

    if (m_nightConditions == nextConditions) {
        emit nightConditionsChanged();
        return;
    }

    m_nightConditions = nextConditions;
    emit nightConditionsChanged();
}
