#include "SkyEphemerisAssetStatus.hpp"

#include <QStringLiteral>

#include <utility>

namespace skygate::ui::internal {
namespace {

constexpr QLatin1StringView kInstalledPrefix("Installed: ");
constexpr QLatin1StringView kMissingInstalledText("Bundled fallback (installed data missing)");
constexpr QLatin1StringView kBundledFallbackText("Bundled fallback");
constexpr QLatin1StringView kBundledDisplayText("Bundled");

}  // namespace

SkyEphemerisAssetStatus::SkyEphemerisAssetStatus(Source source, QString version)
    : m_source(source), m_version(std::move(version))
{
}

SkyEphemerisAssetStatus SkyEphemerisAssetStatus::bundledFallback()
{
    return SkyEphemerisAssetStatus(Source::BundledFallback, {});
}

SkyEphemerisAssetStatus SkyEphemerisAssetStatus::missingInstalledFallback()
{
    return SkyEphemerisAssetStatus(Source::MissingInstalledFallback, {});
}

SkyEphemerisAssetStatus SkyEphemerisAssetStatus::installed(QString version)
{
    return SkyEphemerisAssetStatus(Source::Installed, std::move(version));
}

SkyEphemerisAssetStatus SkyEphemerisAssetStatus::fromStatusText(const QString& statusText)
{
    const QString normalizedText = statusText.trimmed();
    if (normalizedText.startsWith(kInstalledPrefix)) {
        return installed(normalizedText.mid(kInstalledPrefix.size()));
    }
    if (normalizedText.startsWith(kMissingInstalledText)) {
        return missingInstalledFallback();
    }
    return bundledFallback();
}

SkyEphemerisAssetStatus::Source SkyEphemerisAssetStatus::source() const noexcept
{
    return m_source;
}

bool SkyEphemerisAssetStatus::isInstalled() const noexcept
{
    return m_source == Source::Installed;
}

QString SkyEphemerisAssetStatus::version() const
{
    return m_version;
}

QString SkyEphemerisAssetStatus::statusText() const
{
    switch (m_source) {
    case Source::Installed:
        return QStringLiteral("Installed: %1").arg(m_version);
    case Source::MissingInstalledFallback:
        return QString(kMissingInstalledText);
    case Source::BundledFallback:
        break;
    }
    return QString(kBundledFallbackText);
}

QString SkyEphemerisAssetStatus::displayText() const
{
    return m_source == Source::Installed ? m_version : QString(kBundledDisplayText);
}

}  // namespace skygate::ui::internal
