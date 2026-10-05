#pragma once

#include "catalog/constellation/ConstellationData.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace skygate::ui::internal {

// One constellation reference dataset: the line references, anchor groups,
// and declared constellation count held by a single owner. A source instance
// owns the dataset its related download produced; the runtime holds the
// composed active view in the same form.
//
// revision() increases whenever the dataset content changes, so a caller can
// tell how often an owner's data was replaced and whether a pending operation
// observed the same revision.
class SkyCatalogConstellationStore final {
public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationAnchorGroup = skygate::ephemeris::ConstellationAnchorGroup;

    // Replaces the whole dataset in one coherent update. Returns true when the
    // stored content changed.
    [[nodiscard]] bool setDataset(
        std::vector<ConstellationLineRef> lineRefs,
        std::vector<ConstellationAnchorGroup> anchorGroups,
        std::size_t count
    );

    // Drops every stored reference. Returns true when the stored content
    // changed.
    [[nodiscard]] bool clear();

    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::size_t count() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> lineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationAnchorGroup> anchorGroups() const noexcept;
    [[nodiscard]] const std::vector<ConstellationLineRef>& lineRefVector() const noexcept;
    [[nodiscard]] const std::vector<ConstellationAnchorGroup>& anchorGroupVector() const noexcept;

private:
    std::vector<ConstellationLineRef> m_lineRefs;
    std::vector<ConstellationAnchorGroup> m_anchorGroups;
    std::size_t m_count = 0;
    std::uint64_t m_revision = 0U;
};

}  // namespace skygate::ui::internal
