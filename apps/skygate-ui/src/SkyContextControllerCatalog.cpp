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
