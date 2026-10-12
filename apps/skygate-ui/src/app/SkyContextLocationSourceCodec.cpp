#include "SkyContextLocationSourceCodec.hpp"
#include "SkyContextLocationSource.hpp"

namespace skygate::ui::internal {

QString SkyContextLocationSourceCodec::toString(const SkyContextLocationSource locationSource)
{
    switch (locationSource) {
    case SkyContextLocationSource::CurrentDevice:
        return "Current Device";
    case SkyContextLocationSource::City:
        return "City";
    case SkyContextLocationSource::Custom:
        return "Custom";
    }

    return "Custom";
}

std::optional<SkyContextLocationSource> SkyContextLocationSourceCodec::fromString(const QString& value)
{
    const QString normalized = value.trimmed().toCaseFolded();
    if (normalized == "current device") {
        return SkyContextLocationSource::CurrentDevice;
    }

    if (normalized == "city") {
        return SkyContextLocationSource::City;
    }

    if (normalized == "custom") {
        return SkyContextLocationSource::Custom;
    }

    return std::nullopt;
}

QStringList SkyContextLocationSourceCodec::availableOptions()
{
    QStringList options;
#if SKYGATE_HAS_POSITIONING
    options.push_back(toString(SkyContextLocationSource::CurrentDevice));
#endif
    options.push_back(toString(SkyContextLocationSource::City));
    options.push_back(toString(SkyContextLocationSource::Custom));
    return options;
}

SkyContextLocationSource SkyContextLocationSourceCodec::defaultSource()
{
#if SKYGATE_HAS_POSITIONING
    return SkyContextLocationSource::CurrentDevice;
#else
    return SkyContextLocationSource::Custom;
#endif
}

bool SkyContextLocationSourceCodec::isAvailable(const SkyContextLocationSource locationSource)
{
#if SKYGATE_HAS_POSITIONING
    (void)locationSource;
    return true;
#else
    return locationSource != SkyContextLocationSource::CurrentDevice;
#endif
}

}  // namespace skygate::ui::internal
