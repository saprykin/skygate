#include "skygate/core/SystemTimeSource.hpp"

#include <chrono>

namespace skygate::core {

UtcTimePoint SystemTimeSource::nowUtc() const noexcept
{
    return std::chrono::time_point_cast<std::chrono::seconds>(
        std::chrono::system_clock::now()
    );
}

}  // namespace skygate::core
