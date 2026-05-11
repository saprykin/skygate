#pragma once

#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

using ConstellationLineRef = std::pair<std::string, std::string>;
using ConstellationLabelRef = std::pair<std::string, std::vector<std::string>>;

}  // namespace skygate::ephemeris
