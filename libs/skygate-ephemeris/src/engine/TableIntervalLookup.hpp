#pragma once

#include <algorithm>
#include <cstddef>
#include <span>

namespace skygate::ephemeris {

/// Locates the rows that a lookup at one key reaches in a table sorted by key.
/// The table-backed providers and the samplers search their rows the same way,
/// so the search and its edge cases live here; each caller keeps the sampling
/// or estimation it builds on the located rows.
class TableIntervalLookup final {
public:
    /// Rows around one requested key. `upperIndex` names the first row whose
    /// key is not less than the requested key, or the row count when every row
    /// key is below it.
    struct Result {
        std::size_t upperIndex = 0U;
        std::size_t rowCount = 0U;

        /// A row reaches the requested key.
        [[nodiscard]] bool hasUpperRow() const noexcept
        {
            return upperIndex < rowCount;
        }

        /// A row sits at or below the requested key.
        [[nodiscard]] bool hasLowerRow() const noexcept
        {
            return upperIndex > 0U;
        }
    };

    TableIntervalLookup() = delete;

    /// The row span must be sorted by `keyOf` and must not be empty.
    template <typename Entry, typename KeyOf>
    [[nodiscard]] static Result
    locate(const std::span<const Entry> rows, const double requestedKey, KeyOf keyOf) noexcept
    {
        const auto upper = std::ranges::lower_bound(rows, requestedKey, {}, keyOf);
        return Result{
            .upperIndex = static_cast<std::size_t>(upper - rows.begin()),
            .rowCount = rows.size(),
        };
    }
};

}  // namespace skygate::ephemeris
