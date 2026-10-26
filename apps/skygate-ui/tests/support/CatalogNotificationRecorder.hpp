#pragma once

#include "catalog/SkyCatalogManager.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace skygate::ui::tests {

// Records the state a consumer reads from the manager on every catalogChanged
// notification, so a test can prove that a notification never published a
// state the runtime then silently moved away from.
class CatalogNotificationRecorder final {
public:
    struct State final {
        std::uint64_t revision = 0;
        std::size_t lineRefCount = 0;
        std::size_t anchorGroupCount = 0;
        std::size_t constellationCount = 0;
        std::size_t bodyCount = 0;
    };

    static void observe(SkyCatalogManager& manager, std::vector<State>& observations);
    static void assertMatchesRuntime(const SkyCatalogManager& manager, const State& observation);
};

}  // namespace skygate::ui::tests
