#pragma once

#include "SkyTheme.hpp"

#include <QVariantList>
#include <QVariantMap>

#include <span>

class SkyThemeOptionsAdapter final {
public:
    [[nodiscard]] QVariantList themeOptions(
        const std::span<const skygate::ui::internal::SkyThemeOption> options
    ) const
    {
        QVariantList entries;
        entries.reserve(static_cast<qsizetype>(options.size()));
        for (const skygate::ui::internal::SkyThemeOption& option : options) {
            QVariantMap entry;
            entry.insert("id", option.id);
            entry.insert("label", option.label);
            entries.push_back(entry);
        }
        return entries;
    }
};
