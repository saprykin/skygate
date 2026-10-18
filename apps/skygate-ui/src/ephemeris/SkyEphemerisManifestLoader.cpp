#include "SkyEphemerisManifestLoader.hpp"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>

#include <cstddef>
#include <string_view>
#include <utility>

namespace skygate::ui::internal {

SkyEphemerisManifestLoader::Result
SkyEphemerisManifestLoader::load(const QString& path, const std::optional<std::uint64_t> maximumBytes)
{
    Result result;
    QFile manifestFile(path);
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.status = QFileInfo::exists(path) ? Status::OpenFailed : Status::Missing;
        result.fileErrorText = manifestFile.errorString();
        return result;
    }

    result.byteCount = manifestFile.size();
    if (maximumBytes.has_value() && result.byteCount > static_cast<qint64>(*maximumBytes)) {
        result.status = Status::TooLarge;
        return result;
    }

    const QByteArray payload = manifestFile.readAll();
    skygate::ephemeris::EphemerisDataManifest::ParseResult parseResult =
        skygate::ephemeris::EphemerisDataManifest::parse(
            std::string_view(payload.constData(), static_cast<std::size_t>(payload.size()))
        );
    if (!parseResult.isSuccess()) {
        result.status = Status::ParseFailed;
        result.diagnostics = std::move(parseResult.diagnostics);
        return result;
    }

    result.status = Status::Loaded;
    result.manifest = std::move(parseResult.manifest);
    return result;
}

}  // namespace skygate::ui::internal
