#pragma once

#include "catalog/CatalogLoadDiagnostics.hpp"
#include "catalog/IStarCatalog.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace skygate::ephemeris {

struct CatalogLoadResult {
    enum class PayloadFormat : std::uint8_t {
        HygCsv,
        HygCsvGzip,
        HygCsvZip,
        OpenNgcCsv,
        Unknown
    };

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
    PayloadFormat detectedFormat = PayloadFormat::Unknown;
    ErrorCode errorCode = ErrorCode::NoError;
    std::string errorDetail;
    CatalogLoadDiagnostics diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return catalog != nullptr;
    }
};

}  // namespace skygate::ephemeris
