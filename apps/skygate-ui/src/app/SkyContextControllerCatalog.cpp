#include "SkyContextController.hpp"
#include "catalog/SkyCatalogManager.hpp"
#include "catalog/SkyCatalogPresets.hpp"
#include "catalog/SkyCatalogSourceCollectionModel.hpp"
#include "catalog/SkyCatalogSourcePresetModel.hpp"
#include "search/SkyObjectSearchModel.hpp"

#include <span>
#include <utility>
#include <vector>

using namespace skygate::ui::internal;

void SkyContextController::addCatalogSourcePreset(const QString& presetId)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->addSourcePreset(presetId);
    }
}

void SkyContextController::addCatalogSourceUrl(const QString& urlText, const QString& category)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->addSourceUrl(urlText, category);
    }
}

void SkyContextController::enableCatalogSource(const QString& instanceId)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->enableSource(instanceId);
    }
}

void SkyContextController::disableCatalogSource(const QString& instanceId)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->disableSource(instanceId);
    }
}

void SkyContextController::removeCatalogSource(const QString& instanceId)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->removeSource(instanceId);
    }
}

void SkyContextController::moveCatalogSource(const QString& instanceId, const int targetIndex)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->moveSource(instanceId, targetIndex);
    }
}

void SkyContextController::retryCatalogSource(const QString& instanceId)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->retrySource(instanceId);
    }
}

bool SkyContextController::clearCatalogSourceCache(const QString& instanceId)
{
    return m_catalogManager != nullptr && m_catalogManager->clearSourceCache(instanceId);
}

QHash<QString, QString> SkyContextController::catalogSourceTitles() const
{
    return m_catalogManager != nullptr ? m_catalogManager->sourceTitles() : QHash<QString, QString>{};
}

std::span<const QString> SkyContextController::catalogSourceIds() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->sourceIds() : std::span<const QString>{};
}

const std::vector<QStringList>& SkyContextController::catalogSourceContributorIds() const noexcept
{
    static const std::vector<QStringList> kEmptyContributors;
    return m_catalogManager != nullptr ? m_catalogManager->contributorSourceIds() : kEmptyContributors;
}

std::span<const SkyContextController::ConstellationAnchorGroup>
SkyContextController::constellationAnchorGroups() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->constellationAnchorGroups()
                                       : std::span<const ConstellationAnchorGroup>{};
}

std::span<const SkyContextController::ConstellationLineRef> SkyContextController::resolvedConstellationLineRefs() const
{
    return m_catalogManager != nullptr ? m_catalogManager->resolvedConstellationLineRefs()
                                       : std::span<const ConstellationLineRef>{};
}

std::span<const SkyContextController::ConstellationAnchorGroup>
SkyContextController::resolvedConstellationAnchorGroups() const
{
    return m_catalogManager != nullptr ? m_catalogManager->resolvedConstellationAnchorGroups()
                                       : std::span<const ConstellationAnchorGroup>{};
}

int SkyContextController::catalogPresetIndex() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->catalogPresetIndex() : 0;
}

int SkyContextController::deepSkyCatalogPresetIndex() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->deepSkyCatalogPresetIndex() : 0;
}

void SkyContextController::refreshObjectSearchModel()
{
    if (m_objectSearchModel == nullptr) {
        return;
    }

    m_objectSearchModel->setCatalogData(catalogBodies(), resolvedConstellationAnchorGroups());
}

void SkyContextController::refreshCatalogSourceCollectionModel()
{
    if (m_catalogSourceCollectionModel == nullptr || m_catalogManager == nullptr) {
        return;
    }

    QVector<SkyCatalogSourceCollectionModel::SourceEntry> entries;
    const QVector<SkyCatalogManager::SourceViewEntry> sourceView = m_catalogManager->sourceViewEntries();
    entries.reserve(sourceView.size());
    for (const SkyCatalogManager::SourceViewEntry& viewEntry : sourceView) {
        SkyCatalogSourceCollectionModel::SourceEntry entry;
        entry.instanceId = viewEntry.instanceId;
        entry.title = viewEntry.title;
        entry.version = viewEntry.version;
        entry.category = SkyCatalogPresets::categoryForPolicy(viewEntry.policy);
        entry.enabled = viewEntry.enabled;
        entry.bundled = viewEntry.bundled;
        entry.busy = viewEntry.busy;
        entry.hasError = viewEntry.hasError;
        entry.statusText = viewEntry.statusText;
        entry.objectCount = static_cast<int>(viewEntry.objectCount);
        entries.push_back(std::move(entry));
    }
    m_catalogSourceCollectionModel->replaceEntries(std::move(entries));
}

void SkyContextController::setCatalogPresetIndex(const int catalogPresetIndex)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->setCatalogPresetIndex(catalogPresetIndex);
    }
}

void SkyContextController::setDeepSkyCatalogPresetIndex(const int deepSkyCatalogPresetIndex)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->setDeepSkyCatalogPresetIndex(deepSkyCatalogPresetIndex);
    }
}

QString SkyContextController::catalogUrlText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->catalogUrlText() : QString();
}

void SkyContextController::setCatalogUrlText(const QString& catalogUrlText)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->setCatalogUrlText(catalogUrlText);
    }
}

QString SkyContextController::deepSkyCatalogUrlText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->deepSkyCatalogUrlText() : QString();
}

void SkyContextController::setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->setDeepSkyCatalogUrlText(deepSkyCatalogUrlText);
    }
}

QString SkyContextController::catalogSourcePresetId(const int index) const
{
    return m_catalogSourcePresetModel != nullptr ? m_catalogSourcePresetModel->sourceIdAt(index) : QString();
}

QString SkyContextController::catalogSourcePresetCategory(const int index) const
{
    return m_catalogSourcePresetModel != nullptr ? m_catalogSourcePresetModel->categoryAt(index) : QString();
}

bool SkyContextController::catalogSourcePresetIsCustom(const int index) const
{
    return m_catalogSourcePresetModel != nullptr && m_catalogSourcePresetModel->isCustomAt(index);
}
