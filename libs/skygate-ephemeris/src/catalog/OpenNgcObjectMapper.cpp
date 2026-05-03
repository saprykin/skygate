#include "catalog/OpenNgcObjectMapper.hpp"

#include "catalog/CatalogParsingUtilities.hpp"
#include "common/StringUtilities.hpp"

#include <QRegularExpression>
#include <QStringList>

namespace skygate::ephemeris {
namespace {

QString normalizedCatalogAlias(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    text.replace('_', ' ');
    text.replace('-', ' ');
    text = text.simplified();

    const auto normalizePrefix = [&text](const QString& prefix) {
        if (!text.startsWith(prefix, Qt::CaseInsensitive)) {
            return;
        }

        QString suffix = text.sliced(prefix.size()).trimmed();
        suffix = OpenNgcObjectMapper::withoutLeadingZeros(suffix);
        if (!suffix.isEmpty()) {
            text = QString("%1 %2").arg(prefix, suffix);
        }
    };

    normalizePrefix("NGC");
    normalizePrefix("IC");
    normalizePrefix("M");
    return text;
}

QString objectIdFromAlias(const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (normalized.startsWith("NGC ", Qt::CaseInsensitive)) {
        return "ngc_" + normalized.sliced(4).trimmed();
    }
    if (normalized.startsWith("IC ", Qt::CaseInsensitive)) {
        return "ic_" + normalized.sliced(3).trimmed();
    }
    if (normalized.startsWith("M ", Qt::CaseInsensitive)) {
        return "messier_" + normalized.sliced(2).trimmed().rightJustified(3, '0');
    }

    QString id = normalized.toLower();
    id.replace(QRegularExpression("[^a-z0-9]+"), "_");
    while (id.startsWith('_')) {
        id.remove(0, 1);
    }
    while (id.endsWith('_')) {
        id.chop(1);
    }
    return id.isEmpty() ? QString {} : "open_ngc_" + id;
}

void appendAlias(std::vector<std::string>& aliases, const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (normalized.isEmpty()) {
        return;
    }

    strings::appendUniqueIgnoreAsciiCase(aliases, catalog_parsing::toUtf8String(normalized));
}

void appendDelimitedAliases(std::vector<std::string>& aliases, const QString& text)
{
    for (const QString& token : text.split(',', Qt::SkipEmptyParts)) {
        appendAlias(aliases, token);
    }
}

DeepSkyObjectKind kindFromOpenNgcType(QString typeText)
{
    typeText = typeText.trimmed();
    if (typeText == "G" || typeText == "GGroup" || typeText == "GPair" || typeText == "GTrpl") {
        return DeepSkyObjectKind::Galaxy;
    }
    if (typeText == "OCl" || typeText == "Cl+N") {
        return DeepSkyObjectKind::OpenCluster;
    }
    if (typeText == "GCl") {
        return DeepSkyObjectKind::GlobularCluster;
    }
    if (typeText == "PN") {
        return DeepSkyObjectKind::PlanetaryNebula;
    }
    if (
        typeText == "Neb"
        || typeText == "HII"
        || typeText == "EmN"
        || typeText == "RfN"
        || typeText == "SNR"
    ) {
        return DeepSkyObjectKind::Nebula;
    }
    if (typeText == "*Ass") {
        return DeepSkyObjectKind::Asterism;
    }
    return DeepSkyObjectKind::Unknown;
}

}  // namespace

bool OpenNgcObjectMapper::shouldSkipType(const QString& typeText)
{
    return typeText == "NonEx" || typeText == "Dup" || typeText == "Star";
}

QString OpenNgcObjectMapper::withoutLeadingZeros(const QString& value)
{
    QString trimmed = value.trimmed();
    while (trimmed.size() > 1 && trimmed.startsWith('0')) {
        trimmed.remove(0, 1);
    }
    return trimmed;
}

OpenNgcObjectMapping OpenNgcObjectMapper::mapObject(
    const QString& typeText,
    const QString& name,
    const QString& messier,
    const QString& ngc,
    const QString& ic,
    const QString& identifiers,
    const QString& commonNames
)
{
    OpenNgcObjectMapping mapping;
    if (!messier.isEmpty()) {
        appendAlias(mapping.aliases, "M " + messier);
    }
    if (!ngc.isEmpty()) {
        appendAlias(mapping.aliases, "NGC " + ngc);
    }
    if (!ic.isEmpty()) {
        appendAlias(mapping.aliases, "IC " + ic);
    }
    appendAlias(mapping.aliases, name);
    appendDelimitedAliases(mapping.aliases, identifiers);
    appendDelimitedAliases(mapping.aliases, commonNames);

    const QString displayName = !messier.isEmpty()
        ? QString("M%1").arg(messier)
        : (!ngc.isEmpty()
            ? QString("NGC %1").arg(ngc)
            : (!ic.isEmpty() ? QString("IC %1").arg(ic) : normalizedCatalogAlias(name)));
    const QString id = !messier.isEmpty()
        ? objectIdFromAlias("M " + messier)
        : (!ngc.isEmpty()
            ? objectIdFromAlias("NGC " + ngc)
            : (!ic.isEmpty() ? objectIdFromAlias("IC " + ic) : objectIdFromAlias(name)));

    mapping.id = catalog_parsing::toUtf8String(id);
    mapping.displayName = catalog_parsing::toUtf8String(displayName);
    mapping.kind = kindFromOpenNgcType(typeText);
    return mapping;
}

}  // namespace skygate::ephemeris
