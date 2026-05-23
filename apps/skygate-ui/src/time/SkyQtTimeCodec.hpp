#pragma once

#include "UtcTimePoint.hpp"

#include <QDate>
#include <QDateTime>
#include <QString>

class SkyQtTimeCodec final {
public:
    [[nodiscard]] static QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime);
    [[nodiscard]] static skygate::core::UtcTimePoint toUtcTimePoint(const QDateTime& utcDateTime);
    [[nodiscard]] static QString formatDateText(const QDate& date);
    [[nodiscard]] static QString formatDateTimeText(const QDateTime& dateTime, bool includeSeconds = true);
};
