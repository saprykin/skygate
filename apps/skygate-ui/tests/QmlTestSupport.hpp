#pragma once

#include "SkyContextController.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkySceneModel.hpp"
#include "SkyViewportItem.hpp"

#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include <QtTest/QtTest>

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QMetaProperty>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QWheelEvent>
#include <qqml.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>

#ifndef SKYGATE_QML_SOURCE_DIR
#define SKYGATE_QML_SOURCE_DIR ""
#endif

namespace skygate::ui::tests {

class QmlWarningScope final {
public:
    enum class Forwarding {
        Enabled,
        Disabled
    };

    explicit QmlWarningScope(const Forwarding forwarding = Forwarding::Enabled) :
        m_forwarding(forwarding)
    {
        s_currentScope = this;
        m_previousHandler = qInstallMessageHandler(&QmlWarningScope::messageHandler);
    }

    ~QmlWarningScope()
    {
        qInstallMessageHandler(m_previousHandler);
        s_currentScope = nullptr;
    }

    [[nodiscard]] QStringList messages() const
    {
        return m_messages;
    }

private:
    static void messageHandler(
        const QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
    )
    {
        if (s_currentScope != nullptr && type == QtWarningMsg) {
            s_currentScope->m_messages.push_back(message);
        }

        if (
            s_currentScope != nullptr
            && s_currentScope->m_forwarding == Forwarding::Enabled
            && s_currentScope->m_previousHandler != nullptr
        ) {
            s_currentScope->m_previousHandler(type, context, message);
        }
    }

private:
    Forwarding m_forwarding = Forwarding::Enabled;
    QtMessageHandler m_previousHandler = nullptr;
    QStringList m_messages;
    static inline QmlWarningScope* s_currentScope = nullptr;
};

class QmlSettingsFixture final {
public:
    [[nodiscard]] bool initialize(const QString& applicationName)
    {
        if (!m_settingsDir.isValid()) {
            return false;
        }

        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QCoreApplication::setOrganizationName(QStringLiteral("SkygateTests"));
        QCoreApplication::setApplicationName(applicationName);
        return true;
    }

    void resetForCurrentTest()
    {
        QSettings settings;
        settings.clear();
        settings.setValue(
            QStringLiteral("skyContext/catalogCachePath"),
            cachePath(QStringLiteral("star-cache.csv"))
        );
        settings.setValue(
            QStringLiteral("skyContext/deepSkyCatalogCachePath"),
            cachePath(QStringLiteral("deep-sky-cache.csv"))
        );
    }

    [[nodiscard]] QString cachePath(const QString& fileName) const
    {
        const QString testName = QString::fromUtf8(QTest::currentTestFunction()).replace(':', '_');
        return m_settingsDir.filePath(testName + QStringLiteral("-") + fileName);
    }

private:
    QTemporaryDir m_settingsDir;
};

class ExposedQuickWindow final {
public:
    explicit ExposedQuickWindow(QQuickItem* item)
    {
        m_window.resize(900, 640);
        item->setParentItem(m_window.contentItem());
        item->setWidth(m_window.width());
        item->setHeight(m_window.height());
        m_window.show();
        (void)QTest::qWaitForWindowExposed(&m_window);
        QCoreApplication::processEvents();
    }

    [[nodiscard]] QQuickWindow* window() noexcept
    {
        return &m_window;
    }

private:
    QQuickWindow m_window;
};

class TestSkyViewportItem final : public SkyViewportItem {
public:
    [[nodiscard]] QSGNode* buildPaintNode()
    {
        return updatePaintNode(nullptr, nullptr);
    }
};

inline int nodeTreeSize(const QSGNode* node)
{
    if (node == nullptr) {
        return 0;
    }

    int count = 1;
    for (const QSGNode* child = node->firstChild(); child != nullptr; child = child->nextSibling()) {
        count += nodeTreeSize(child);
    }
    return count;
}

inline void collectObjectTree(QObject* object, QList<QObject*>& objects, QSet<QObject*>& visited)
{
    if (object == nullptr || visited.contains(object)) {
        return;
    }

    visited.insert(object);
    objects.push_back(object);
    for (QObject* child : object->children()) {
        collectObjectTree(child, objects, visited);
    }

    if (auto* item = qobject_cast<QQuickItem*>(object)) {
        for (QQuickItem* childItem : item->childItems()) {
            collectObjectTree(childItem, objects, visited);
        }
    }
}

inline QList<QObject*> objectTree(QObject* root)
{
    QList<QObject*> objects;
    QSet<QObject*> visited;
    collectObjectTree(root, objects, visited);
    return objects;
}

inline QObject* firstObjectWithProperty(
    QObject* root,
    const char* propertyName,
    const QVariant& value
)
{
    for (QObject* object : objectTree(root)) {
        if (object->property(propertyName) == value) {
            return object;
        }
    }
    return nullptr;
}

inline QList<QObject*> objectsWithMetaProperty(QObject* root, const char* propertyName)
{
    QList<QObject*> matches;
    for (QObject* object : objectTree(root)) {
        if (object->metaObject()->indexOfProperty(propertyName) >= 0) {
            matches.push_back(object);
        }
    }
    return matches;
}

inline QObject* firstObjectWithMetaProperties(
    QObject* root,
    const std::initializer_list<const char*> propertyNames
)
{
    for (QObject* object : objectTree(root)) {
        const QMetaObject* metaObject = object->metaObject();
        const bool hasAllProperties = std::all_of(
            propertyNames.begin(),
            propertyNames.end(),
            [metaObject](const char* propertyName) {
                return metaObject->indexOfProperty(propertyName) >= 0;
            }
        );
        if (hasAllProperties) {
            return object;
        }
    }
    return nullptr;
}

inline QList<QObject*> objectsWithText(QObject* root, const QString& text)
{
    QList<QObject*> matches;
    for (QObject* object : objectTree(root)) {
        if (object->property("text").toString() == text) {
            matches.push_back(object);
        }
    }
    return matches;
}

inline QObject* firstObjectWithObjectName(QObject* root, const QString& objectName)
{
    for (QObject* object : objectTree(root)) {
        if (object->objectName() == objectName) {
            return object;
        }
    }
    return nullptr;
}

inline QList<QObject*> invokableButtonsWithText(QObject* root, const QString& text)
{
    QList<QObject*> matches;
    for (QObject* object : objectsWithText(root, text)) {
        if (object->metaObject()->indexOfSignal("clicked()") >= 0) {
            matches.push_back(object);
        }
    }
    return matches;
}

inline QList<QObject*> comboBoxesWithCount(QObject* root, const int count)
{
    QList<QObject*> matches;
    for (QObject* object : objectTree(root)) {
        if (
            object->metaObject()->indexOfProperty("currentIndex") >= 0
            && object->property("count").toInt() == count
        ) {
            matches.push_back(object);
        }
    }
    return matches;
}

inline QQuickItem* firstVisibleItemWithText(QObject* root, const QString& text)
{
    for (QObject* object : objectsWithText(root, text)) {
        auto* item = qobject_cast<QQuickItem*>(object);
        if (item != nullptr && item->isVisible()) {
            return item;
        }
    }
    return nullptr;
}

inline QQuickItem* firstVisibleItemContainingText(QObject* root, const QString& text)
{
    for (QObject* object : objectTree(root)) {
        auto* item = qobject_cast<QQuickItem*>(object);
        if (
            item != nullptr
            && item->isVisible()
            && object->property("text").toString().contains(text)
        ) {
            return item;
        }
    }
    return nullptr;
}

inline QQuickItem* quickItemProperty(QObject* object, const char* propertyName)
{
    return qobject_cast<QQuickItem*>(qvariant_cast<QObject*>(object->property(propertyName)));
}

inline QList<QObject*> objectsWithPlaceholder(QObject* root, const QString& placeholderText)
{
    QList<QObject*> matches;
    for (QObject* object : objectTree(root)) {
        if (object->property("placeholderText").toString() == placeholderText) {
            matches.push_back(object);
        }
    }
    return matches;
}

inline QList<QObject*> checkBoxes(QObject* root)
{
    QList<QObject*> matches;
    for (QObject* object : objectTree(root)) {
        const QMetaObject* metaObject = object->metaObject();
        if (
            metaObject->indexOfProperty("checked") >= 0
            && metaObject->indexOfSignal("toggled()") >= 0
            && metaObject->indexOfMethod("toggle()") >= 0
        ) {
            matches.push_back(object);
        }
    }
    return matches;
}

inline void commitText(QWindow* window, const QString& text)
{
    QInputMethodEvent event;
    event.setCommitString(text);
    QCoreApplication::sendEvent(window, &event);
    QCoreApplication::processEvents();
}

inline void replaceText(QWindow* window, QQuickItem* item, const QString& text)
{
    QVERIFY(item != nullptr);
    QVERIFY(QMetaObject::invokeMethod(item, "forceActiveFocus"));
    QVERIFY(QMetaObject::invokeMethod(item, "selectAll"));
    commitText(window, text);
}

inline bool activateControl(QObject* object)
{
    if (object == nullptr) {
        return false;
    }

    if (object->metaObject()->indexOfMethod("click()") >= 0) {
        return QMetaObject::invokeMethod(object, "click");
    }
    if (object->metaObject()->indexOfSignal("clicked()") >= 0) {
        return QMetaObject::invokeMethod(object, "clicked");
    }
    return false;
}

inline QString qmlSourcePath(const QString& fileName)
{
    return QStringLiteral(SKYGATE_QML_SOURCE_DIR) + QStringLiteral("/") + fileName;
}

inline QString componentErrors(const QQmlComponent& component)
{
    QStringList messages;
    for (const QQmlError& error : component.errors()) {
        messages.push_back(error.toString());
    }
    return messages.join('\n');
}

inline bool writeFile(const QString& path, const QByteArray& contents)
{
    const QFileInfo fileInfo(path);
    QDir().mkpath(fileInfo.absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

inline std::unique_ptr<QObject> createFileComponent(
    QQmlEngine& engine,
    const QString& fileName,
    const QVariantMap& initialProperties = {}
)
{
    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlSourcePath(fileName)));
    if (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    if (component.isError()) {
        qWarning().noquote() << componentErrors(component);
        return {};
    }

    return std::unique_ptr<QObject>(
        component.createWithInitialProperties(initialProperties, engine.rootContext())
    );
}

inline std::unique_ptr<QObject> createInlineComponent(
    QQmlEngine& engine,
    const QString& source,
    const QString& fileName = QStringLiteral("InlineTest.qml")
)
{
    QQmlComponent component(&engine);
    component.setData(source.toUtf8(), QUrl::fromLocalFile(qmlSourcePath(fileName)));
    if (component.isError()) {
        qWarning().noquote() << componentErrors(component);
        return {};
    }

    return std::unique_ptr<QObject>(component.create(engine.rootContext()));
}

inline std::unique_ptr<SkyContextController> makeController()
{
    auto starCatalog = skygate::ephemeris::createBundledStarCatalog();
    if (starCatalog == nullptr) {
        return {};
    }
    auto ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*starCatalog);
    if (ephemerisEngine == nullptr) {
        return {};
    }

    return std::make_unique<SkyContextController>(
        std::move(starCatalog),
        std::move(ephemerisEngine)
    );
}

inline std::unique_ptr<SkySceneModel> makeSceneModel(SkyContextController& controller)
{
    auto sceneModel = std::make_unique<SkySceneModel>();
    sceneModel->setSkyContextController(&controller);
    return sceneModel;
}

inline bool catalogContainsDisplayName(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const QString& displayName
)
{
    return std::any_of(
        bodies.begin(),
        bodies.end(),
        [&displayName](const skygate::ephemeris::CelestialBody& body) {
            return QString::fromStdString(body.displayName) == displayName;
        }
    );
}

inline bool catalogContainsAlias(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const QString& alias
)
{
    return std::any_of(
        bodies.begin(),
        bodies.end(),
        [&alias](const skygate::ephemeris::CelestialBody& body) {
            if (!body.deepSkyObject.has_value()) {
                return false;
            }
            return std::any_of(
                body.deepSkyObject->aliases.begin(),
                body.deepSkyObject->aliases.end(),
                [&alias](const std::string& bodyAlias) {
                    return QString::fromStdString(bodyAlias) == alias;
                }
            );
        }
    );
}

struct SkyViewRenderSignature final {
    int sceneGraphNodeCount = 0;
    int sceneGraphVertexCount = 0;
    std::size_t renderLineCount = 0;
    std::size_t renderPointCount = 0;
    std::size_t renderGlyphCount = 0;
    int overlayItemCount = 0;
    SkyOverlayLayerVisibility overlayLayers;

    [[nodiscard]] bool differsFrom(const SkyViewRenderSignature& other) const noexcept
    {
        return sceneGraphNodeCount != other.sceneGraphNodeCount
            || sceneGraphVertexCount != other.sceneGraphVertexCount
            || renderLineCount != other.renderLineCount
            || renderPointCount != other.renderPointCount
            || renderGlyphCount != other.renderGlyphCount
            || overlayItemCount != other.overlayItemCount
            || !overlayLayers.equals(other.overlayLayers);
    }
};

inline int geometryVertexCount(const QSGNode* node)
{
    if (node == nullptr) {
        return 0;
    }

    int vertexCount = 0;
    if (node->type() == QSGNode::GeometryNodeType) {
        const auto* geometryNode = static_cast<const QSGGeometryNode*>(node);
        if (geometryNode->geometry() != nullptr) {
            vertexCount += geometryNode->geometry()->vertexCount();
        }
    }

    for (const QSGNode* child = node->firstChild(); child != nullptr; child = child->nextSibling()) {
        vertexCount += geometryVertexCount(child);
    }
    return vertexCount;
}

inline SkyViewRenderSignature renderSignature(
    SkyContextController& controller,
    SkySceneModel& sceneModel
)
{
    TestSkyViewportItem viewport;
    viewport.setWidth(760.0);
    viewport.setHeight(520.0);
    viewport.setSkySceneModel(&sceneModel);
    for (int attempt = 0; attempt < 100 && sceneModel.snapshotGeneration() == 0U; ++attempt) {
        QCoreApplication::processEvents();
        QTest::qWait(10);
    }

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    return SkyViewRenderSignature {
        .sceneGraphNodeCount = nodeTreeSize(paintNode.get()),
        .sceneGraphVertexCount = geometryVertexCount(paintNode.get()),
        .renderLineCount = sceneModel.renderLineSpan().size(),
        .renderPointCount = sceneModel.renderPointSpan().size(),
        .renderGlyphCount = sceneModel.renderGlyphSpan().size(),
        .overlayItemCount = static_cast<int>(sceneModel.overlayItems().size()),
        .overlayLayers = controller.overlayLayerVisibility()
    };
}

inline void setupEngine(
    QQmlEngine& engine,
    SkyContextController& controller,
    SkySceneModel* sceneModel = nullptr
)
{
    engine.addImportPath(QStringLiteral(SKYGATE_QML_SOURCE_DIR));
    engine.rootContext()->setContextProperty("skyContext", &controller);
    if (sceneModel != nullptr) {
        engine.rootContext()->setContextProperty("skyScene", sceneModel);
    }
}

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

#define SKYGATE_QML_TEST_MAIN(TestClass) \
    int main(int argc, char* argv[]) \
    { \
        skygate::ui::tests::configureQmlTestProcess(); \
        QGuiApplication app(argc, argv); \
        skygate::ui::tests::configureQmlTestApplication(app); \
        TestClass tests; \
        return QTest::qExec(&tests, argc, argv); \
    }
