#pragma once

#include "SettingsTestFixture.hpp"

#include <QString>

namespace skygate::ui::tests {

class QmlSettingsFixture final {
public:
    [[nodiscard]] bool initialize(const QString& applicationName)
    {
        return m_settings.initialize(applicationName);
    }

    void resetForCurrentTest()
    {
        m_settings.resetForCurrentTest();
    }

    [[nodiscard]] QString cachePath(const QString& fileName) const
    {
        return m_settings.currentTestFilePath(fileName);
    }

private:
    SettingsTestFixture m_settings;
};

}  // namespace skygate::ui::tests
