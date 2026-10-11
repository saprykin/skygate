#include "EphemerisDataFileUtilities.hpp"
#include "StringUtilities.hpp"

#include <QFileInfo>

namespace skygate::ephemeris {

QString EphemerisDataFileUtilities::pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

bool EphemerisDataFileUtilities::hasUnsafePathComponent(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute()) {
        return true;
    }

    for (const std::filesystem::path& component : path) {
        if (component.empty() || component == "." || component == "..") {
            return true;
        }
    }

    return false;
}

bool EphemerisDataFileUtilities::sourceSizeMatchesMetadata(
    const QFileInfo& sourceInfo, const EphemerisDataManifest::Asset& asset
)
{
    return !asset.compression.compressedSizeBytes.has_value()
           || sourceInfo.size() == static_cast<qint64>(*asset.compression.compressedSizeBytes);
}

bool EphemerisDataFileUtilities::cancellationRequested(const std::function<bool()>& callback)
{
    return callback != nullptr && callback();
}

bool EphemerisDataFileUtilities::checksumsMatch(const std::string_view actual, const std::string_view expected) noexcept
{
    return StringUtilities::equalsIgnoreAsciiCase(actual, expected);
}

}  // namespace skygate::ephemeris
