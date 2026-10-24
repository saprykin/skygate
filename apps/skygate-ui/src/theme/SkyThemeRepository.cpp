#include "SkyThemeRepository.hpp"
#include "SkyThemeDefinition.hpp"
#include "SkyThemeJsonCodec.hpp"
#include "SkyThemeOption.hpp"
#include "SkyThemeOptionsAdapter.hpp"
#include "diagnostics/SkyLoggingCategories.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>

void initSkygateThemeResources()
{
    Q_INIT_RESOURCE(themes);
}

namespace skygate::ui::internal {
namespace {

[[nodiscard]] std::optional<QByteArray> readResourceBytes(const QString& resourcePath, const QString& description)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(skygateThemeLog).noquote() << "Failed to open" << description << resourcePath;
        return std::nullopt;
    }
    return file.readAll();
}

void ensureThemeResourcesInitialized()
{
    static QMutex mutex;
    static bool initialized = false;

    const QMutexLocker locker(&mutex);
    if (initialized) {
        return;
    }

    initSkygateThemeResources();
    initialized = true;
}

}  // namespace

SkyThemeRepository::SkyThemeRepository(const QString& manifestResourcePath)
    : m_fallbackTheme(SkyThemeDefinition::builtInDefault())
{
    ensureThemeResourcesInitialized();
    loadManifest(manifestResourcePath);
    ensureDefaultTheme();
}

const SkyThemeDefinition& SkyThemeRepository::themeById(const QString& id) const noexcept
{
    const QString requestedId = id.trimmed();
    if (!requestedId.isEmpty()) {
        for (const auto& theme : m_themes) {
            if (theme.id == requestedId) {
                return theme;
            }
        }

        qCWarning(skygateThemeLog).noquote() << "Unknown theme id" << requestedId << "- using default theme";
    }

    return defaultTheme();
}

const SkyThemeDefinition& SkyThemeRepository::defaultTheme() const noexcept
{
    if (m_defaultThemeIndex >= 0 && m_defaultThemeIndex < static_cast<qsizetype>(m_themes.size())) {
        return m_themes[static_cast<std::size_t>(m_defaultThemeIndex)];
    }
    return m_fallbackTheme;
}

QVariantList SkyThemeRepository::themeOptions() const
{
    const auto options = themeOptionData();
    const SkyThemeOptionsAdapter adapter;
    return adapter.themeOptions(options);
}

std::vector<SkyThemeOption> SkyThemeRepository::themeOptionData() const
{
    std::vector<SkyThemeOption> options;
    options.reserve(m_themes.size());
    for (const auto& theme : m_themes) {
        options.push_back(SkyThemeOption{.id = theme.id, .label = theme.displayName});
    }

    if (options.empty()) {
        options.push_back(SkyThemeOption{.id = m_fallbackTheme.id, .label = m_fallbackTheme.displayName});
    }
    return options;
}

void SkyThemeRepository::loadManifest(const QString& manifestResourcePath)
{
    const std::optional<QByteArray> manifestBytes = readResourceBytes(manifestResourcePath, "theme manifest");
    if (!manifestBytes.has_value()) {
        return;
    }

    const std::optional<QJsonObject> manifestObject = SkyThemeJsonCodec::parseObject(
        manifestBytes.value(), QStringLiteral("theme manifest %1").arg(manifestResourcePath)
    );
    if (!manifestObject.has_value()) {
        return;
    }

    const QString defaultThemeId = manifestObject->value("defaultThemeId").toString().trimmed();
    const QJsonArray themeArray = manifestObject->value("themes").toArray();
    for (const QJsonValue& themeValue : themeArray) {
        if (!themeValue.isObject()) {
            continue;
        }

        const QJsonObject themeObject = themeValue.toObject();
        const QString descriptorDescription =
            QStringLiteral("theme descriptor in manifest %1").arg(manifestResourcePath);
        const std::optional<QString> descriptorId =
            SkyThemeJsonCodec::requiredString(themeObject, "id", descriptorDescription);
        const std::optional<QString> descriptorResourcePath =
            SkyThemeJsonCodec::requiredString(themeObject, "resourcePath", descriptorDescription);
        if (!descriptorId.has_value() || !descriptorResourcePath.has_value()) {
            continue;
        }

        const QString id = descriptorId.value();
        const QString resourcePath = descriptorResourcePath.value();

        const std::optional<QByteArray> themeBytes = readResourceBytes(resourcePath, "theme");
        if (!themeBytes.has_value()) {
            continue;
        }

        const std::optional<SkyThemeDefinition> parsedTheme =
            SkyThemeJsonCodec::parseTheme(themeBytes.value(), resourcePath);
        if (!parsedTheme.has_value()) {
            continue;
        }

        if (parsedTheme->id != id) {
            qCWarning(skygateThemeLog).noquote()
                << "Theme manifest id mismatch for" << resourcePath << "- expected" << id << "got" << parsedTheme->id;
            continue;
        }

        if (parsedTheme->id == defaultThemeId) {
            m_defaultThemeIndex = static_cast<qsizetype>(m_themes.size());
        }
        m_themes.push_back(parsedTheme.value());
    }
}

void SkyThemeRepository::ensureDefaultTheme()
{
    if (m_themes.empty()) {
        m_themes.push_back(m_fallbackTheme);
        m_defaultThemeIndex = 0;
        return;
    }

    if (m_defaultThemeIndex < 0 || m_defaultThemeIndex >= static_cast<qsizetype>(m_themes.size())) {
        for (qsizetype index = 0; index < static_cast<qsizetype>(m_themes.size()); ++index) {
            if (m_themes[static_cast<std::size_t>(index)].id == m_fallbackTheme.id) {
                m_defaultThemeIndex = index;
                return;
            }
        }

        m_defaultThemeIndex = 0;
    }
}

}  // namespace skygate::ui::internal
