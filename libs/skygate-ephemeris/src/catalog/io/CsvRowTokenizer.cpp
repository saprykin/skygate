#include "catalog/io/CsvRowTokenizer.hpp"

#include <QtGlobal>

namespace skygate::ephemeris {

QVector<QStringView> CsvRowTokenizer::splitColumns(const QStringView line)
{
    return splitColumns(line, ',');
}

QVector<QStringView> CsvRowTokenizer::splitColumns(const QStringView line, const QChar separator)
{
    QVector<QStringView> columns;
    columns.reserve(32);

    bool insideQuotes = false;
    qsizetype columnStart = 0;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == '"') {
            if (insideQuotes && i + 1 < line.size() && line.at(i + 1) == '"') {
                ++i;
                continue;
            }
            insideQuotes = !insideQuotes;
            continue;
        }

        if (c == separator && !insideQuotes) {
            columns.push_back(line.sliced(columnStart, i - columnStart));
            columnStart = i + 1;
        }
    }

    columns.push_back(line.sliced(columnStart));
    return columns;
}

QString CsvRowTokenizer::decodeField(QStringView field)
{
    field = field.trimmed();
    if (field.size() < 2 || !field.startsWith('"') || !field.endsWith('"')) {
        return field.toString();
    }

    field = field.sliced(1, field.size() - 2);
    if (!field.contains(QStringLiteral("\"\""))) {
        return field.toString();
    }

    QString decoded;
    decoded.reserve(field.size());
    for (qsizetype i = 0; i < field.size(); ++i) {
        const QChar c = field.at(i);
        if (c == '"' && i + 1 < field.size() && field.at(i + 1) == '"') {
            decoded.push_back('"');
            ++i;
            continue;
        }
        decoded.push_back(c);
    }

    return decoded;
}

}  // namespace skygate::ephemeris
