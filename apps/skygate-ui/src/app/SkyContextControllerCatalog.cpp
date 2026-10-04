#include "SkyContextController.hpp"

#include "SkyCatalogManager.hpp"

void SkyContextController::loadCatalogPreset(const QString& presetId)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->loadCatalogPreset(presetId);
    }
}

void SkyContextController::downloadCatalogFromUrl(const QString& urlText)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->downloadCatalogFromUrl(urlText);
    }
}

void SkyContextController::loadDeepSkyCatalogPreset(const QString& presetId)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->loadDeepSkyCatalogPreset(presetId);
    }
}

void SkyContextController::downloadDeepSkyCatalogFromUrl(const QString& urlText)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->downloadDeepSkyCatalogFromUrl(urlText);
    }
}

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
