#pragma once

#include "QmlRenderingTestSupport.hpp"

class QmlOverlayRenderingTestBase : public QObject {
protected:
    [[nodiscard]] bool initializeOverlayRenderingSettings(const QString& testName)
    {
        return m_settings.initialize(testName);
    }

    void resetOverlayRenderingSettings()
    {
        m_settings.resetForCurrentTest();
    }

private:
    QmlSettingsFixture m_settings;
};
