#pragma once

#include <QString>
#include <QStringView>

namespace skygate::ui::internal {

// Profile ids used by the ephemeris data manifest and the persisted user
// settings. The legacy `modern` id names the same short-range profile.
class SkyEphemerisDataProfileIds final {
public:
    SkyEphemerisDataProfileIds() = delete;

    static constexpr QStringView kDefaultProfileId = u"de440s-short-range";

    [[nodiscard]] static QString canonical(const QString& profileId)
    {
        const QString trimmedProfileId = profileId.trimmed();
        if (trimmedProfileId == QStringLiteral("modern")) {
            return QString(kDefaultProfileId);
        }
        return trimmedProfileId;
    }
};

}  // namespace skygate::ui::internal
