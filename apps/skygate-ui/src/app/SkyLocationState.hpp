#pragma once

#include "ObservationContext.hpp"
#include "SkyContextLocationSource.hpp"
#include "SkyContextLocationSourceCodec.hpp"

#include <QString>

namespace skygate::ui::internal {

struct SkyLocationState final {
    skygate::core::ObservationContext context;
    SkyContextLocationSource source = SkyContextLocationSourceCodec::defaultSource();
    QString statusText;
    QString selectedCityId;
    QString selectedCityDisplayText;
};

}  // namespace skygate::ui::internal
