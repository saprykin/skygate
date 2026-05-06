#pragma once

#include "SkyViewportItem.hpp"

#include <QFont>
#include <QGuiApplication>
#include <QtGlobal>
#include <qqml.h>

namespace skygate::ui::tests {

inline void configureQmlTestProcess()
{
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
}

inline void configureQmlTestApplication(QGuiApplication& app)
{
    qmlRegisterType<SkyViewportItem>("com.skygate.app", 1, 0, "SkyViewportItem");
    QFont::insertSubstitution(QStringLiteral("Sans Serif"), QStringLiteral("Avenir Next"));
    QFont::insertSubstitution(QStringLiteral("Monospace"), QStringLiteral("Menlo"));
    app.setFont(QFont(QStringLiteral("Avenir Next")));
}

}  // namespace skygate::ui::tests
