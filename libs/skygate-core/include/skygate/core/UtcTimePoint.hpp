#pragma once

#include <chrono>

namespace skygate::core {

using UtcTimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;

}  // namespace skygate::core
