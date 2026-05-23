#pragma once

#include "catalog/CatalogLoadDiagnostics.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "catalog/IStarCatalog.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace skygate::ephemeris {

struct CatalogLoadResult {
    enum class ErrorCode : std::uint8_t {
        NoError,
        EmptyInput,
        UnsupportedFormat,
        MissingRequiredColumns,
        InvalidHygCsv,
        InvalidOpenNgcCsv,
        InvalidGzipData,
        InvalidZipData,
        NoBodies
    };

    std::unique_ptr<IStarCatalog> catalog;
    CatalogSourceType detectedFormat = CatalogSourceType::Unknown;
    ErrorCode errorCode = ErrorCode::NoError;
    std::string errorDetail;
    CatalogLoadDiagnostics diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return catalog != nullptr;
    }
};

}  // namespace skygate::ephemeris
