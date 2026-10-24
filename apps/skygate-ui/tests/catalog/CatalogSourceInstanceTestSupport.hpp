#pragma once

#include "catalog/SkyCatalogPresets.hpp"
#include "catalog/SkyCatalogSourceDescriptor.hpp"
#include "catalog/SkyCatalogSourceInstance.hpp"

#include <QString>

#include <optional>

namespace skygate::ui::tests {

// Builds the source instance a catalog preset configures. A test that models
// an explicit reload or update of an existing source passes that source's
// durable instance ID, so the load replaces the instance instead of adding a
// sibling.
[[nodiscard]] inline skygate::ui::internal::SkyCatalogSourceInstance
presetSourceInstance(const QString& presetId, const QString& instanceId = {}, const bool deepSky = false)
{
    const std::optional<skygate::ui::internal::SkyCatalogSourceDescriptor> descriptor =
        deepSky ? skygate::ui::internal::SkyCatalogPresets::deepSkySourceDescriptor(presetId)
                : skygate::ui::internal::SkyCatalogPresets::starSourceDescriptor(presetId);
    if (!descriptor.has_value()) {
        return {};
    }

    skygate::ui::internal::SkyCatalogSourceInstance instance =
        skygate::ui::internal::SkyCatalogSourceInstance::fromDescriptor(*descriptor);
    if (!instanceId.isEmpty()) {
        instance.instanceId = instanceId;
    }
    return instance;
}

[[nodiscard]] inline skygate::ui::internal::SkyCatalogSourceInstance
customSourceInstance(const QString& urlText, const QString& instanceId = {})
{
    skygate::ui::internal::SkyCatalogSourceInstance instance =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(urlText);
    if (!instanceId.isEmpty()) {
        instance.instanceId = instanceId;
    }
    return instance;
}

}  // namespace skygate::ui::tests
