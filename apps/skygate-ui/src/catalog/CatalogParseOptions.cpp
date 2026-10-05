#include "CatalogParseOptions.hpp"

#include <string_view>
#include <utility>

skygate::ephemeris::CatalogParseRequest CatalogParseOptions::makeRequest(
    const std::string_view payload, skygate::ephemeris::CatalogParseProgressCallback progressCallback
) const
{
    skygate::ephemeris::CatalogParseRequest request;
    request.payload = payload;
    request.progressCallback = std::move(progressCallback);
    request.selectionOptions = selectionOptions;
    request.schemaHint = schemaHint;
    if (!archiveMember.isEmpty()) {
        request.memberSelector = archiveMember.toStdString();
    }
    return request;
}
