#pragma once

#include "skygate/ephemeris/ConstellationData.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace skygate::ui::internal {

class SkyCatalogConstellationStore final {
public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationLabelRef = skygate::ephemeris::ConstellationLabelRef;

    void clear();
    void setLineRefs(std::vector<ConstellationLineRef> lineRefs);
    void setLabelRefs(std::vector<ConstellationLabelRef> labelRefs);
    void setCount(std::size_t count) noexcept;

    [[nodiscard]] std::size_t count() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> lineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationLabelRef> labelRefs() const noexcept;
    [[nodiscard]] const std::vector<ConstellationLineRef>& lineRefVector() const noexcept;
    [[nodiscard]] const std::vector<ConstellationLabelRef>& labelRefVector() const noexcept;

private:
    std::vector<ConstellationLineRef> m_lineRefs;
    std::vector<ConstellationLabelRef> m_labelRefs;
    std::size_t m_count = 0;
};

}  // namespace skygate::ui::internal
