#include "SkySceneOverlayAdapter.hpp"

namespace {

QVariantMap overlayItemMap(const SkyOverlayItem& item)
{
    QVariantMap entry;
    if (!item.kind.isEmpty()) {
        entry.insert("kind", item.kind);
    }
    entry.insert("x", item.x);
    entry.insert("y", item.y);
    entry.insert("text", item.text);
    entry.insert("color", item.color);
    return entry;
}

QVariantMap inspectorFieldMap(const SkyInspectorField& field)
{
    QVariantMap entry;
    entry.insert("label", field.label);
    entry.insert("value", field.value);
    return entry;
}

}  // namespace

QVariantList SkySceneOverlayAdapter::overlayItems(
    const std::span<const SkyOverlayItem> overlayItems
) const
{
    QVariantList items;
    items.reserve(static_cast<qsizetype>(overlayItems.size()));
    for (const SkyOverlayItem& item : overlayItems) {
        items.push_back(overlayItemMap(item));
    }
    return items;
}

QVariantMap SkySceneOverlayAdapter::selectionMarker(
    const SkySelectionMarker& marker
) const
{
    if (!marker.visible) {
        return {};
    }

    QVariantMap entry;
    entry.insert("kind", marker.kind);
    entry.insert("x", marker.x);
    entry.insert("y", marker.y);
    return entry;
}

QVariantMap SkySceneOverlayAdapter::selectedObjectInspector(
    const SkySelectedObjectInspector& inspector
) const
{
    if (!inspector.visible) {
        return {};
    }

    QVariantList fields;
    fields.reserve(static_cast<qsizetype>(inspector.fields.size()));
    for (const SkyInspectorField& field : inspector.fields) {
        fields.push_back(inspectorFieldMap(field));
    }

    QVariantMap entry;
    entry.insert("visible", true);
    entry.insert("x", inspector.x);
    entry.insert("y", inspector.y);
    entry.insert("targetKind", inspector.targetKind);
    entry.insert("targetId", inspector.targetId);
    entry.insert("title", inspector.title);
    entry.insert("pinned", inspector.pinned);
    entry.insert("fields", fields);
    entry.insert("aliases", inspector.aliases);
    return entry;
}
