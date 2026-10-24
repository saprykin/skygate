#pragma once

#include "SkyThemeDefinition.hpp"
#include "SkyThemeOption.hpp"

#include <QString>
#include <QVariantList>

#include <vector>

namespace skygate::ui::internal {

class SkyThemeRepository final {
public:
    explicit SkyThemeRepository(const QString& manifestResourcePath = QStringLiteral(":/themes/manifest.json"));

    [[nodiscard]] const SkyThemeDefinition& themeById(const QString& id) const noexcept;
    [[nodiscard]] const SkyThemeDefinition& defaultTheme() const noexcept;
    [[nodiscard]] QVariantList themeOptions() const;

private:
    [[nodiscard]] std::vector<SkyThemeOption> themeOptionData() const;
    void loadManifest(const QString& manifestResourcePath);
    void ensureDefaultTheme();

private:
    std::vector<SkyThemeDefinition> m_themes;
    SkyThemeDefinition m_fallbackTheme;
    qsizetype m_defaultThemeIndex = 0;
};

}  // namespace skygate::ui::internal
