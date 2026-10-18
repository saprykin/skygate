#pragma once

#include <QString>

// Spells a value as one path segment: trims surrounding whitespace, keeps
// letters, digits, '-', '_', and '.', replaces other characters with '_', and
// collapses repeated dots. An empty result, or one that is only '.', becomes
// the caller's fallback. Containment checks and filesystem operations stay
// with the ephemeris file utilities.
namespace skygate::ui::internal {

class SkyEphemerisPathSegmentPolicy final {
public:
    [[nodiscard]] static QString spellPathSegment(const QString& value, const QString& fallback);
};

}  // namespace skygate::ui::internal
