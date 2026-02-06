#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>

namespace skygate::ephemeris {

[[nodiscard]] std::unique_ptr<IStarCatalog> createBundledStarCatalog();

}  // namespace skygate::ephemeris
