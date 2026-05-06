#pragma once

#include "QmlTestPaths.hpp"
#include "SkyContextController.hpp"
#include "SkySceneModel.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <memory>

namespace skygate::ui::tests {

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

}  // namespace skygate::ui::tests
