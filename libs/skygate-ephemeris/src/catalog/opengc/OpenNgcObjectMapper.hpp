#pragma once

#include "OpenNgcObjectMapping.hpp"

#include <QString>

namespace skygate::ephemeris {

class OpenNgcObjectMapper final {
public:
    [[nodiscard]] static bool shouldSkipType(const QString& typeText);
    [[nodiscard]] static QString withoutLeadingZeros(const QString& value);
    [[nodiscard]] static OpenNgcObjectMapping mapObject(
        const QString& typeText,
        const QString& name,
        const QString& messier,
        const QString& ngc,
        const QString& ic,
        const QString& identifiers,
        const QString& commonNames
    );
};

}  // namespace skygate::ephemeris
