#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace skygate::ephemeris {

enum class CatalogPayloadFormat : std::uint8_t {
    PipeRows,
    HygCsv,
    HygCsvGzip,
    HygCsvZip,
    Unknown
};

enum class CatalogLoadErrorCode : std::uint8_t {
    None,
    EmptyInput,
    UnsupportedFormat,
    InvalidRows,
    MissingRequiredColumns,
    InvalidHygCsv,
    InvalidGzipData,
    InvalidZipData,
    NoBodies
};

struct CatalogLoadDiagnostics {
    std::size_t processedRowCount = 0;
    std::size_t parsedBodyCount = 0;
    std::size_t selectedBodyCount = 0;
    std::size_t truncatedBodyCount = 0;
};

struct CatalogLoadResult {
    std::unique_ptr<IStarCatalog> catalog;
    CatalogPayloadFormat detectedFormat = CatalogPayloadFormat::Unknown;
    CatalogLoadErrorCode errorCode = CatalogLoadErrorCode::None;
    std::string errorDetail;
    CatalogLoadDiagnostics diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return catalog != nullptr;
    }
};

}  // namespace skygate::ephemeris
