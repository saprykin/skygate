#pragma once

#include "IStarCatalog.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace skygate::ephemeris {

// Result of composing an ordered source collection.
//
// sourceIds is parallel to catalog->bodies(): each entry is the stable source
// instance ID of the source that contributed the surviving body. When a body
// is merged from several sources, the winning source's ID is recorded.
// contributorSourceIds is parallel to sourceIds and lists every source that
// contributed to the surviving body in precedence order.
//
// The count fields are all derived from this single composition result so
// consumers do not re-derive them from source catalogs. bodyCount is the
// number of unique surviving objects; sourceOrder/sourceRowCounts describe how
// many surviving rows each source contributed; the per-kind counts describe
// the surviving snapshot by object kind.
//
// A rejected collection leaves catalog null, isSuccess() false, and records
// the reason in errorCode/errorDetail. errorCode is NoError and errorDetail is
// empty for every accepted collection.
struct CatalogCompositionResult final {
    enum class ErrorCode : std::uint8_t {
        NoError,
        InvalidSourceIdentity
    };

    std::unique_ptr<IStarCatalog> catalog;
    std::vector<std::string> sourceIds;
    std::vector<std::vector<std::string>> contributorSourceIds;
    std::size_t bodyCount = 0;
    std::size_t constellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    std::size_t foundDeepSkyObjectCount = 0;
    std::size_t starCount = 0;
    std::size_t planetCount = 0;
    std::size_t moonCount = 0;
    std::size_t sunCount = 0;
    std::vector<std::string> sourceOrder;
    std::vector<std::size_t> sourceRowCounts;
    ErrorCode errorCode = ErrorCode::NoError;
    std::string errorDetail;

    [[nodiscard]] bool isSuccess() const noexcept;
};

}  // namespace skygate::ephemeris
