#pragma once

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QQuickItem>
#include <QSet>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <initializer_list>

namespace skygate::ui::tests {

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

inline QQuickItem* firstQuickItemWithObjectName(QObject* root, const QString& objectName)
{
    return qobject_cast<QQuickItem*>(firstObjectWithObjectName(root, objectName));
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

}  // namespace skygate::ui::tests
