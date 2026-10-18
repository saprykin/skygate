#include "SkyContextProjectionTypeCodec.hpp"
#include "projections/ProjectionTypeCodec.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <string_view>
#include <vector>

namespace skygate::ui::internal {

QString SkyContextProjectionTypeCodec::toString(const skygate::core::ProjectionType projectionType)
{
    const std::string_view projectionTypeText = skygate::core::ProjectionTypeCodec::text(projectionType);
    if (projectionTypeText.empty()) {
        return QStringLiteral("Unknown");
    }
    return QString::fromUtf8(projectionTypeText.data(), static_cast<qsizetype>(projectionTypeText.size()));
}

std::optional<skygate::core::ProjectionType> SkyContextProjectionTypeCodec::fromString(const QString& value)
{
    const QByteArray utf8 = value.trimmed().toUtf8();
    return skygate::core::ProjectionTypeCodec::fromText(
        std::string_view(utf8.constData(), static_cast<std::size_t>(utf8.size()))
    );
}

QStringList SkyContextProjectionTypeCodec::availableOptions()
{
    QStringList options;
    const std::vector<skygate::core::ProjectionType> values = skygate::core::ProjectionTypeCodec::orderedValues();
    options.reserve(static_cast<qsizetype>(values.size()));
    for (const skygate::core::ProjectionType value : values) {
        options.push_back(toString(value));
    }
    return options;
}

}  // namespace skygate::ui::internal
