#pragma once

#include <QString>

namespace skygate::ui::internal {

struct SkyLocationSettingsSnapshot final {
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double elevationMeters = 0.0;
    QString sourceText;
    QString selectedCityId;
    QString displayTimeZoneId;
};

}  // namespace skygate::ui::internal
