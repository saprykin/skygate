#pragma once

#include <QString>
#include <QStringList>

class TimeZoneAliasCatalog final {
public:
    [[nodiscard]] static QStringList aliasesForTimeZoneId(const QString& timeZoneId);
};
