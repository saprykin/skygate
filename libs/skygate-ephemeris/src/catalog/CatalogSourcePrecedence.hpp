#pragma once

#include <QtGlobal>

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace skygate::ephemeris {

// Configured precedence of every source in a composition collection, keyed by
// source instance id. A source's rank is its position in the configured
// collection: a later source outranks every earlier source, exactly like a
// replacement winner outranks the survivor it absorbs. The rank is never
// derived from the accumulator position of a survivor, because replacement and
// bridge absorption vacate accumulator positions and append the winner at a
// new position, so position order stops matching source precedence after the
// first replacement.
class CatalogSourcePrecedence final {
public:
    void add(const std::string_view sourceId, const std::size_t rank)
    {
        m_ranks.insert_or_assign(std::string{sourceId}, rank);
    }

    [[nodiscard]] std::size_t rankOf(const std::string_view sourceId) const
    {
        const auto found = m_ranks.find(std::string{sourceId});
        Q_ASSERT(found != m_ranks.end());
        return found != m_ranks.end() ? found->second : 0U;
    }

    [[nodiscard]] bool outranks(const std::string_view lhsSourceId, const std::string_view rhsSourceId) const
    {
        return rankOf(lhsSourceId) > rankOf(rhsSourceId);
    }

private:
    std::unordered_map<std::string, std::size_t> m_ranks;
};

}  // namespace skygate::ephemeris
