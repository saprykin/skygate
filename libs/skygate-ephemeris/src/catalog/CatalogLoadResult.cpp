#include "CatalogLoadResult.hpp"

namespace skygate::ephemeris {

std::string_view CatalogLoadResult::describeErrorCode(const ErrorCode errorCode) noexcept
{
    switch (errorCode) {
    case ErrorCode::NoError:
        return "Unknown parse failure";
    case ErrorCode::EmptyInput:
        return "empty payload";
    case ErrorCode::UnsupportedFormat:
        return "unsupported format";
    case ErrorCode::MissingRequiredColumns:
        return "missing required columns";
    case ErrorCode::InvalidHygCsv:
        return "invalid HYG CSV payload";
    case ErrorCode::InvalidOpenNgcCsv:
        return "invalid OpenNGC CSV payload";
    case ErrorCode::InvalidGzipData:
        return "invalid gzip payload";
    case ErrorCode::InvalidZipData:
        return "invalid ZIP payload";
    case ErrorCode::NoBodies:
        return "catalog contains no usable bodies";
    case ErrorCode::ArchiveMemberNotFound:
        return "requested archive member not found";
    case ErrorCode::AmbiguousArchiveMember:
        return "multiple supported archive members";
    case ErrorCode::SchemaHintMismatch:
        return "schema hint mismatch";
    }

    return "Unknown parse failure";
}

}  // namespace skygate::ephemeris
