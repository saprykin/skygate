#include "CatalogNotificationRecorder.hpp"

#include <QObject>
#include <QtTest/QtTest>

namespace skygate::ui::tests {

void CatalogNotificationRecorder::observe(SkyCatalogManager& manager, std::vector<State>& observations)
{
    QObject::connect(&manager, &SkyCatalogManager::catalogChanged, &manager, [&manager, &observations] {
        observations.push_back(
            State{
                .revision = manager.catalogRevision(),
                .lineRefCount = manager.constellationLineRefs().size(),
                .anchorGroupCount = manager.constellationAnchorGroups().size(),
                .constellationCount = manager.constellationCount(),
                .bodyCount = manager.starCatalog()->bodies().size(),
            }
        );
    });
}

void CatalogNotificationRecorder::assertMatchesRuntime(const SkyCatalogManager& manager, const State& observation)
{
    QCOMPARE(observation.revision, manager.catalogRevision());
    QCOMPARE(observation.lineRefCount, manager.constellationLineRefs().size());
    QCOMPARE(observation.anchorGroupCount, manager.constellationAnchorGroups().size());
    QCOMPARE(observation.constellationCount, manager.constellationCount());
    QCOMPARE(observation.bodyCount, manager.starCatalog()->bodies().size());
}

}  // namespace skygate::ui::tests
