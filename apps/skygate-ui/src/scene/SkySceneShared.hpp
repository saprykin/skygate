#pragma once

#include <QString>

#include <string>

[[nodiscard]] inline QString normalizedSceneLookupKey(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

[[nodiscard]] inline QString normalizedSceneLookupKey(const std::string& value)
{
    return normalizedSceneLookupKey(QString::fromStdString(value));
}
