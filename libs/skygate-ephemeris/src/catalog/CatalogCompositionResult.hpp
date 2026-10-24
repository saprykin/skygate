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
// bodyCount is the number of unique surviving objects and deepSkyObjectCount
// is the surviving deep-sky object count; both are derived from the composed
// snapshot so publishing consumers do not re-derive them from source catalogs.
// foundDeepSkyObjectCount carries the pre-merge deep-sky figure reported by the
// sources, and sourceOrder lists the distinct contributing sources in
// precedence order without duplicates.
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
    std::size_t deepSkyObjectCount = 0;
    std::size_t foundDeepSkyObjectCount = 0;
    std::vector<std::string> sourceOrder;
    ErrorCode errorCode = ErrorCode::NoError;
    std::string errorDetail;

    [[nodiscard]] bool isSuccess() const noexcept;
};

}  // namespace skygate::ephemeris
