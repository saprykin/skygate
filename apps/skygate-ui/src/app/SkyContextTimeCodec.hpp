#pragma once

#include "UtcTimePoint.hpp"

#include <QDateTime>

namespace skygate::ui::internal {

class SkyContextTimeCodec final {
public:
    [[nodiscard]] static QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime);
    [[nodiscard]] static skygate::core::UtcTimePoint toUtcTimePoint(const QDateTime& utcTime);
};

}  // namespace skygate::ui::internal
