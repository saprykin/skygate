#pragma once

#include "SkyOverlayItem.hpp"
#include "SkySelectedObjectInspector.hpp"
#include "SkySelectionMarker.hpp"

#include <QVariantList>
#include <QVariantMap>

#include <span>

class SkySceneOverlayAdapter final {
public:
    [[nodiscard]] QVariantList overlayItems(std::span<const SkyOverlayItem> overlayItems) const;
    [[nodiscard]] QVariantMap selectionMarker(const SkySelectionMarker& marker) const;
    [[nodiscard]] QVariantMap selectedObjectInspector(const SkySelectedObjectInspector& inspector) const;
};
