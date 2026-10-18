#pragma once

#include <QString>

#include <cstdint>

namespace skygate::ui::internal {

// Installed state of one ephemeris data asset. `statusText()` is the string
// the data manager publishes for that asset; `fromStatusText()` decodes the
// same string so callers can compare versions and render availability without
// splitting display text themselves.
class SkyEphemerisAssetStatus final {
public:
    enum class Source : std::uint8_t {
        BundledFallback,
        MissingInstalledFallback,
        Installed,
    };

    [[nodiscard]] static SkyEphemerisAssetStatus bundledFallback();
    [[nodiscard]] static SkyEphemerisAssetStatus missingInstalledFallback();
    [[nodiscard]] static SkyEphemerisAssetStatus installed(QString version);
    [[nodiscard]] static SkyEphemerisAssetStatus fromStatusText(const QString& statusText);

    [[nodiscard]] Source source() const noexcept;
    [[nodiscard]] bool isInstalled() const noexcept;
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString displayText() const;

private:
    SkyEphemerisAssetStatus() = default;
    SkyEphemerisAssetStatus(Source source, QString version);

    Source m_source = Source::BundledFallback;
    QString m_version;
};

}  // namespace skygate::ui::internal
