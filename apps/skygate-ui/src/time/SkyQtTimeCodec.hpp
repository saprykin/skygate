#pragma once

#include "skygate/core/Types.hpp"

#include <QDateTime>

class SkyQtTimeCodec final {
public:
    [[nodiscard]] static QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime);
    [[nodiscard]] static skygate::core::UtcTimePoint toUtcTimePoint(const QDateTime& utcDateTime);
};
