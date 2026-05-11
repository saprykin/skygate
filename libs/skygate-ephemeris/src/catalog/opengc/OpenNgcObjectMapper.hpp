#pragma once

#include "skygate/ephemeris/Types.hpp"

#include <QString>

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct OpenNgcObjectMapping final {
    std::string id;
    std::string displayName;
    DeepSkyObjectKind kind = DeepSkyObjectKind::Unknown;
    std::vector<std::string> aliases;
};

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
