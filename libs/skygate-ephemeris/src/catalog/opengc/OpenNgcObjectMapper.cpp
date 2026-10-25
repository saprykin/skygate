#include "OpenNgcObjectMapper.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogIdentifier.hpp"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {
namespace {

constexpr std::string_view kOpenNgcFallbackNamespace = "open_ngc";

// The matcher shared with CatalogIdentifier decides whether an alias is a
// designation at all: a prefix is one only when a catalog number follows it,
// so labels such as "Maia Nebula" and "MWSC 3156" stay descriptive names.
std::optional<CatalogIdentifier> recognizedDesignation(const QString& text)
{
    return CatalogIdentifier::designationFromText(StringUtilities::toUtf8(text.trimmed()));
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
    text = StringUtilities::normalizedLabelText(text);
    if (text.isEmpty()) {
        return {};
    }

    return QString::fromStdString(CatalogIdentifier::normalizedDesignationText(StringUtilities::toUtf8(text)));
}

QString objectIdFromAlias(const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (const std::optional<CatalogIdentifier> designation =
            CatalogIdentifier::designationFromText(StringUtilities::toUtf8(normalized));
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
    return QString::fromStdString(CatalogIdentifier::bodyId(kOpenNgcFallbackNamespace, StringUtilities::toUtf8(id)));
}

void appendAlias(std::vector<std::string>& aliases, const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (normalized.isEmpty()) {
        return;
    }

    StringUtilities::appendUniqueIgnoreAsciiCase(aliases, StringUtilities::toUtf8(normalized));
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
    return QString::fromStdString(CatalogIdentifier::stripLeadingZeros(StringUtilities::toUtf8(trimmed)));
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

    // The cross-reference columns in catalog precedence order: a Messier number
    // names the object before its NGC number, and the NGC number before an IC
    // number. The first filled column is the object's primary designation.
    const std::array<std::pair<std::string_view, QString>, 3> designationColumns{{
        {CatalogIdentifier::kMessierNamespace, messier},
        {CatalogIdentifier::kNgcNamespace, ngc},
        {CatalogIdentifier::kIcNamespace, ic},
    }};

    std::string_view primaryNamespace;
    QString primaryValue;
    QString primaryAlias;
    for (const auto& [namespaceName, value] : designationColumns) {
        if (value.isEmpty()) {
            continue;
        }

        const QString aliasText = designationAliasText(namespaceName, value);
        appendAlias(mapping.aliases, aliasText);
        mapping.externalIdentifiers.push_back(CatalogIdentifier::make(namespaceName, StringUtilities::toUtf8(value)));
        if (primaryValue.isEmpty()) {
            primaryNamespace = namespaceName;
            primaryValue = value;
            primaryAlias = aliasText;
        }
    }

    // The primary Name column is itself a designation column: a recognized
    // NGC/IC name is authoritative even when the optional cross-reference
    // columns are absent.
    if (const std::optional<CatalogIdentifier> designation = recognizedDesignation(name); designation.has_value()) {
        CatalogIdentifier::appendUnique(mapping.externalIdentifiers, *designation);
    }
    mapping.sourceRecordId = StringUtilities::toUtf8(name);
    appendAlias(mapping.aliases, name);
    appendDelimitedAliases(mapping.aliases, identifiers);
    appendDelimitedAliases(mapping.aliases, commonNames);

    // Messier objects read as "M31"; the other designations keep the spaced
    // catalog spelling their alias uses.
    const QString displayName = !primaryValue.isEmpty() ? (primaryNamespace == CatalogIdentifier::kMessierNamespace
                                                               ? QStringLiteral("M%1").arg(primaryValue)
                                                               : primaryAlias)
                                                        : normalizedCatalogAlias(name);
    const QString id = objectIdFromAlias(primaryValue.isEmpty() ? name : primaryAlias);

    mapping.id = StringUtilities::toUtf8(id);
    mapping.displayName = StringUtilities::toUtf8(displayName);
    mapping.kind = kindFromOpenNgcType(typeText);
    return mapping;
}

}  // namespace skygate::ephemeris
