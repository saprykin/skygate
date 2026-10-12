#include "SkyLocationController.hpp"
#include "SkyContextLocationSource.hpp"
#include "SkySelectedCityChange.hpp"

namespace skygate::ui::internal {

const skygate::core::ObservationContext& SkyLocationController::context() const noexcept
{
    return m_state.context;
}

skygate::core::GeoLocation SkyLocationController::observer() const noexcept
{
    return m_state.context.observer;
}

skygate::core::UtcTimePoint SkyLocationController::utcTime() const noexcept
{
    return m_state.context.utcTime;
}

SkyContextLocationSource SkyLocationController::source() const noexcept
{
    return m_state.source;
}

QString SkyLocationController::statusText() const
{
    return m_state.statusText;
}

QString SkyLocationController::selectedCityId() const
{
    return m_state.selectedCityId;
}

QString SkyLocationController::selectedCityDisplayText() const
{
    return m_state.selectedCityDisplayText;
}

void SkyLocationController::setUtcTime(const skygate::core::UtcTimePoint& utcTime) noexcept
{
    m_state.context.utcTime = utcTime;
}

void SkyLocationController::setObserver(const skygate::core::GeoLocation& observer) noexcept
{
    m_state.context.observer = observer;
}

bool SkyLocationController::setSource(const SkyContextLocationSource source) noexcept
{
    if (m_state.source == source) {
        return false;
    }

    m_state.source = source;
    return true;
}

bool SkyLocationController::setStatusText(const QString& statusText)
{
    if (m_state.statusText == statusText) {
        return false;
    }

    m_state.statusText = statusText;
    return true;
}

SkySelectedCityChange SkyLocationController::clearSelectedCity()
{
    SkySelectedCityChange change{
        .idChanged = !m_state.selectedCityId.isEmpty(), .displayTextChanged = !m_state.selectedCityDisplayText.isEmpty()
    };
    m_state.selectedCityId.clear();
    m_state.selectedCityDisplayText.clear();
    return change;
}

SkySelectedCityChange SkyLocationController::setSelectedCity(const QString& cityId, const QString& displayText)
{
    SkySelectedCityChange change{
        .idChanged = m_state.selectedCityId != cityId,
        .displayTextChanged = m_state.selectedCityDisplayText != displayText
    };
    m_state.selectedCityId = cityId;
    m_state.selectedCityDisplayText = displayText;
    m_state.source = SkyContextLocationSource::City;
    return change;
}

}  // namespace skygate::ui::internal
