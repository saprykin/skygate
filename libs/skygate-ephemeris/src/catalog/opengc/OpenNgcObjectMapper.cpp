#include "OpenNgcObjectMapper.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/io/CatalogParsingUtilities.hpp"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {
namespace {

constexpr std::string_view kOpenNgcFallbackNamespace = "open_ngc";

// The matcher shared with CatalogIdentifier decides whether an alias is a
// designation at all: a prefix is one only when a catalog number follows it,
// so labels such as "Maia Nebula" and "MWSC 3156" stay descriptive names.
std::optional<CatalogIdentifier> recognizedDesignation(const QString& text)
{
    return CatalogIdentifier::designationFromText(CatalogParsingUtilities::toUtf8String(text.trimmed()));
}

// One designation as an alias reads: the prefix CatalogIdentifier knows for the
// namespace, a space, and the catalog number.
QString designationAliasText(const std::string_view namespaceName, const QString& value)
{
    return QString::fromLatin1(CatalogIdentifier::designationPrefix(namespaceName)) + QLatin1Char(' ') + value;
}

// Label normalization is this mapper's display rule; the designation spelling
// itself, including the zero strip and the component suffix, belongs to
// CatalogIdentifier.
QString normalizedCatalogAlias(QString text)
{
    text = CatalogParsingUtilities::normalizedLabelText(text);
    if (text.isEmpty()) {
        return {};
    }

    return QString::fromStdString(
        CatalogIdentifier::normalizedDesignationText(CatalogParsingUtilities::toUtf8String(text))
    );
}

QString objectIdFromAlias(const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (const std::optional<CatalogIdentifier> designation =
            CatalogIdentifier::designationFromText(CatalogParsingUtilities::toUtf8String(normalized));
        designation.has_value()) {
        return QString::fromStdString(CatalogIdentifier::bodyId(designation->namespaceName, designation->value));
    }

    QString id = normalized.toLower();
    id.replace(QRegularExpression("[^a-z0-9]+"), "_");
    while (id.startsWith('_')) {
        id.remove(0, 1);
    }
    while (id.endsWith('_')) {
        id.chop(1);
    }
    if (id.isEmpty()) {
        return {};
    }
    return QString::fromStdString(
        CatalogIdentifier::bodyId(kOpenNgcFallbackNamespace, CatalogParsingUtilities::toUtf8String(id))
    );
}

void appendAlias(std::vector<std::string>& aliases, const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (normalized.isEmpty()) {
        return;
    }

    StringUtilities::appendUniqueIgnoreAsciiCase(aliases, CatalogParsingUtilities::toUtf8String(normalized));
}

void appendDelimitedAliases(std::vector<std::string>& aliases, const QString& text)
{
    for (const QString& token : text.split(',', Qt::SkipEmptyParts)) {
        appendAlias(aliases, token);
    }
}

DeepSkyObjectInfo::Kind kindFromOpenNgcType(const QString& typeText)
{
    if (typeText == "G" || typeText == "GGroup" || typeText == "GPair" || typeText == "GTrpl") {
        return DeepSkyObjectInfo::Kind::Galaxy;
    }
    if (typeText == "OCl" || typeText == "Cl+N") {
        return DeepSkyObjectInfo::Kind::OpenCluster;
    }
    if (typeText == "GCl") {
        return DeepSkyObjectInfo::Kind::GlobularCluster;
    }
    if (typeText == "PN") {
        return DeepSkyObjectInfo::Kind::PlanetaryNebula;
    }
    if (typeText == "Neb" || typeText == "HII" || typeText == "EmN" || typeText == "RfN" || typeText == "SNR") {
        return DeepSkyObjectInfo::Kind::Nebula;
    }
    if (typeText == "*Ass") {
        return DeepSkyObjectInfo::Kind::Asterism;
    }
    return DeepSkyObjectInfo::Kind::Unknown;
}

}  // namespace

bool OpenNgcObjectMapper::shouldSkipType(const QString& typeText)
{
    return typeText == "NonEx" || typeText == "Dup" || typeText == "Star";
}

QString OpenNgcObjectMapper::withoutLeadingZeros(const QString& value)
{
    const QString trimmed = value.trimmed();
    return QString::fromStdString(CatalogIdentifier::stripLeadingZeros(CatalogParsingUtilities::toUtf8String(trimmed)));
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
        appendAlias(mapping.aliases, designationAliasText(CatalogIdentifier::kMessierNamespace, messier));
        mapping.externalIdentifiers.push_back(
            CatalogIdentifier::make(
                CatalogIdentifier::kMessierNamespace, CatalogParsingUtilities::toUtf8String(messier)
            )
        );
    }
    if (!ngc.isEmpty()) {
        appendAlias(mapping.aliases, designationAliasText(CatalogIdentifier::kNgcNamespace, ngc));
        mapping.externalIdentifiers.push_back(
            CatalogIdentifier::make(CatalogIdentifier::kNgcNamespace, CatalogParsingUtilities::toUtf8String(ngc))
        );
    }
    if (!ic.isEmpty()) {
        appendAlias(mapping.aliases, designationAliasText(CatalogIdentifier::kIcNamespace, ic));
        mapping.externalIdentifiers.push_back(
            CatalogIdentifier::make(CatalogIdentifier::kIcNamespace, CatalogParsingUtilities::toUtf8String(ic))
        );
    }
    // The primary Name column is itself a designation column: a recognized
    // NGC/IC name is authoritative even when the optional cross-reference
    // columns are absent.
    if (const std::optional<CatalogIdentifier> designation = recognizedDesignation(name); designation.has_value()) {
        CatalogIdentifier::appendUnique(mapping.externalIdentifiers, *designation);
    }
    mapping.sourceRecordId = CatalogParsingUtilities::toUtf8String(name);
    appendAlias(mapping.aliases, name);
    appendDelimitedAliases(mapping.aliases, identifiers);
    appendDelimitedAliases(mapping.aliases, commonNames);

    const QString displayName =
        !messier.isEmpty()
            ? QString("M%1").arg(messier)
            : (!ngc.isEmpty() ? QString("NGC %1").arg(ngc)
                              : (!ic.isEmpty() ? QString("IC %1").arg(ic) : normalizedCatalogAlias(name)));
    const QString id =
        !messier.isEmpty()
            ? objectIdFromAlias(designationAliasText(CatalogIdentifier::kMessierNamespace, messier))
            : (!ngc.isEmpty()
                   ? objectIdFromAlias(designationAliasText(CatalogIdentifier::kNgcNamespace, ngc))
                   : (!ic.isEmpty() ? objectIdFromAlias(designationAliasText(CatalogIdentifier::kIcNamespace, ic))
                                    : objectIdFromAlias(name)));

    mapping.id = CatalogParsingUtilities::toUtf8String(id);
    mapping.displayName = CatalogParsingUtilities::toUtf8String(displayName);
    mapping.kind = kindFromOpenNgcType(typeText);
    return mapping;
}

}  // namespace skygate::ephemeris
