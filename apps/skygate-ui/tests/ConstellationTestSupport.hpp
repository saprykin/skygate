#pragma once

#include "SettingsTestFixture.hpp"
#include "SkyContextController.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"

#include "skygate/ephemeris/ConstellationData.hpp"

#include <QByteArray>

#include <vector>

namespace skygate::ui::tests {

[[nodiscard]] inline QByteArray orionHygCsvPayload()
{
    return QByteArray(
        "hip,proper,ra,dec,mag\n"
        "27989,HIP 27989,5.5,5.0,1.9\n"
        "25336,HIP 25336,5.5,5.0,2.1\n"
        "25930,HIP 25930,5.5,5.0,2.2\n"
        "26311,HIP 26311,5.5,5.0,2.3\n"
        "26727,HIP 26727,5.5,5.0,2.4\n"
        "24436,HIP 24436,5.5,5.0,2.5\n"
    );
}

[[nodiscard]] inline std::vector<skygate::ephemeris::ConstellationLineRef> orionLineRefs()
{
    return {
        {"hip_27989", "hip_25336"},
        {"hip_25336", "hip_25930"},
        {"hip_25930", "hip_26311"},
        {"hip_26311", "hip_26727"},
        {"hip_26727", "hip_24436"},
    };
}

[[nodiscard]] inline std::vector<skygate::ephemeris::ConstellationLabelRef> orionLabelRefs()
{
    return {
        {
            "Orion",
            {"hip_27989", "hip_25336", "hip_25930", "hip_26311", "hip_26727", "hip_24436"}
        }
    };
}

[[nodiscard]] inline bool seedOrionConstellationCache()
{
    const std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs = orionLineRefs();
    const std::vector<skygate::ephemeris::ConstellationLabelRef> labelRefs = orionLabelRefs();

    SkySettingsStore::CatalogCacheSnapshot snapshot;
    snapshot.sourceLabel = QStringLiteral("Test HYG");
    snapshot.catalogPayload = orionHygCsvPayload();
    snapshot.constellationLineRows =
        skygate::ui::internal::SkyContextCatalogCodec::serializeConstellationLineRows(lineRefs);
    snapshot.constellationLabelRows =
        skygate::ui::internal::SkyContextCatalogCodec::serializeConstellationLabelRows(labelRefs);
    snapshot.constellationLineSchemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
    snapshot.constellationCount = 1U;

    SkySettingsStore::StateSnapshot stateSnapshot;
    stateSnapshot.catalogPresetIndex = 2;

    const SkySettingsStore settingsStore;
    return settingsStore.saveCatalogCache(snapshot) && settingsStore.saveState(stateSnapshot);
}

inline void restoreSeededCatalogCache(SkyContextController& controller)
{
    controller.setCatalogPresetIndex(2);
    static_cast<void>(controller.loadSettings());
}

}  // namespace skygate::ui::tests
