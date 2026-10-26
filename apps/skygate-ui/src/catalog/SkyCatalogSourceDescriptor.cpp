#include "SkyCatalogSourceDescriptor.hpp"

namespace skygate::ui::internal {

bool SkyCatalogSourceDescriptor::isKnown() const noexcept
{
    return !sourceId.isEmpty();
}

QString SkyCatalogSourceDescriptor::defaultUrl() const
{
    return urls.isEmpty() ? QString() : urls.first();
}

}  // namespace skygate::ui::internal
