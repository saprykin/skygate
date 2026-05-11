#pragma once

#include "QmlComponentTestSupport.hpp"
#include "QmlObjectTreeTestSupport.hpp"
#include "QmlSettingsFixture.hpp"
#include "QmlSkyModelTestSupport.hpp"
#include "QmlTestApplication.hpp"
#include "QmlTestPaths.hpp"
#include "QmlViewportRenderTestSupport.hpp"
#include "QmlWarningScope.hpp"
#include "QmlWindowTestSupport.hpp"

#include <QtTest/QtTest>

#define SKYGATE_QML_TEST_MAIN(TestClass) \
    int main(int argc, char* argv[]) \
    { \
        skygate::ui::tests::configureQmlTestProcess(); \
        QGuiApplication app(argc, argv); \
        skygate::ui::tests::configureQmlTestApplication(app); \
        TestClass tests; \
        return QTest::qExec(&tests, argc, argv); \
    }
