#include "SkyConstellationLineImportResult.hpp"

namespace skygate::ui::internal {

bool SkyConstellationLineImportResult::hasCustomLines() const noexcept
{
    return !lineRefs.empty();
}

}  // namespace skygate::ui::internal
