#include "SkyCatalogConstellationStore.hpp"

#include <utility>

namespace skygate::ui::internal {

void SkyCatalogConstellationStore::clear()
{
    m_lineRefs.clear();
    m_labelRefs.clear();
    m_count = 0;
}

void SkyCatalogConstellationStore::setLineRefs(
    std::vector<ConstellationLineRef> lineRefs
)
{
    if (lineRefs.empty()) {
        clear();
        return;
    }

    m_lineRefs = std::move(lineRefs);
}

void SkyCatalogConstellationStore::setLabelRefs(
    std::vector<ConstellationLabelRef> labelRefs
)
{
    m_labelRefs = std::move(labelRefs);
}

void SkyCatalogConstellationStore::setCount(const std::size_t count) noexcept
{
    m_count = count;
}

std::size_t SkyCatalogConstellationStore::count() const noexcept
{
    return m_count;
}

std::span<const SkyCatalogConstellationStore::ConstellationLineRef>
SkyCatalogConstellationStore::lineRefs() const noexcept
{
    return std::span<const ConstellationLineRef>(m_lineRefs);
}

std::span<const SkyCatalogConstellationStore::ConstellationLabelRef>
SkyCatalogConstellationStore::labelRefs() const noexcept
{
    return std::span<const ConstellationLabelRef>(m_labelRefs);
}

const std::vector<SkyCatalogConstellationStore::ConstellationLineRef>&
SkyCatalogConstellationStore::lineRefVector() const noexcept
{
    return m_lineRefs;
}

const std::vector<SkyCatalogConstellationStore::ConstellationLabelRef>&
SkyCatalogConstellationStore::labelRefVector() const noexcept
{
    return m_labelRefs;
}

}  // namespace skygate::ui::internal
