#pragma once

#include "GeoLocation.hpp"
#include "ObservationContext.hpp"
#include "SkyContextLocationSource.hpp"
#include "SkyLocationState.hpp"
#include "SkySelectedCityChange.hpp"
#include "UtcTimePoint.hpp"

#include <QString>

namespace skygate::ui::internal {

class SkyLocationController final {
public:
    [[nodiscard]] const skygate::core::ObservationContext& context() const noexcept;
    [[nodiscard]] skygate::core::GeoLocation observer() const noexcept;
    [[nodiscard]] skygate::core::UtcTimePoint utcTime() const noexcept;
    [[nodiscard]] SkyContextLocationSource source() const noexcept;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString selectedCityId() const;
    [[nodiscard]] QString selectedCityDisplayText() const;

    void setUtcTime(const skygate::core::UtcTimePoint& utcTime) noexcept;
    void setObserver(const skygate::core::GeoLocation& observer) noexcept;
    [[nodiscard]] bool setSource(SkyContextLocationSource source) noexcept;
    [[nodiscard]] bool setStatusText(const QString& statusText);
    [[nodiscard]] SkySelectedCityChange clearSelectedCity();
    [[nodiscard]] SkySelectedCityChange setSelectedCity(const QString& cityId, const QString& displayText);

private:
    SkyLocationState m_state;
};

}  // namespace skygate::ui::internal
