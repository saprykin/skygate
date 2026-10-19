#pragma once

#include "CatalogLoadDiagnostics.hpp"
#include "CatalogSourceType.hpp"
#include "IStarCatalog.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

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
        NoBodies,
        ArchiveMemberNotFound,
        AmbiguousArchiveMember,
        SchemaHintMismatch
    };

    // Wording of an error code for the status texts and the parse log. Codes
    // outside the enumeration carry the same text as NoError.
    [[nodiscard]] static std::string_view describeErrorCode(ErrorCode errorCode) noexcept;

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
