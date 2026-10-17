#include "SkyEphemerisPathSegmentPolicy.hpp"

QString SkyEphemerisPathSegmentPolicy::spellPathSegment(const QString& value, const QString& fallback)
{
    const QString trimmedValue = value.trimmed();
    QString result;
    result.reserve(trimmedValue.size());
    for (const QChar character : trimmedValue) {
        if (character.isLetterOrNumber() || character == QLatin1Char('-') || character == QLatin1Char('_')
            || character == QLatin1Char('.')) {
            result.push_back(character);
        } else {
            result.push_back(QLatin1Char('_'));
        }
    }
    while (result.contains(QStringLiteral(".."))) {
        result.replace(QStringLiteral(".."), QStringLiteral("."));
    }
    return result.isEmpty() || result == QStringLiteral(".") ? fallback : result;
}
