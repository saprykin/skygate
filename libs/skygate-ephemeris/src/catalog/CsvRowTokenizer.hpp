#pragma once

#include <QVector>
#include <QString>
#include <QStringView>

namespace skygate::ephemeris {

class CsvRowTokenizer final {
public:
    [[nodiscard]] static QVector<QStringView> splitColumns(QStringView line);
    [[nodiscard]] static QVector<QStringView> splitColumns(QStringView line, QChar separator);
    [[nodiscard]] static QString decodeField(QStringView field);
};

}  // namespace skygate::ephemeris
