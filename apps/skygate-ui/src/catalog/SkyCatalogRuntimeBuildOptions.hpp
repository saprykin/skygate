#pragma once

#include <cstdint>

namespace skygate::ui::internal {

struct SkyCatalogRuntimeBuildOptions final {
    // Explicit participation of the bundled deep-sky catalog in this
    // composition.
    //
    // Disabled: bundled deep-sky bodies do not participate.
    // Fallback: bundled deep-sky bodies fill only the deep-sky identities no
    // enabled configured source supplies. The fallback never replaces a
    // configured source's body or its position in the visible collection, and
    // its contribution is reported under its own provenance.
    enum class BundledDeepSkyParticipation : std::uint8_t {
        Disabled,
        Fallback
    };

    BundledDeepSkyParticipation bundledDeepSkyParticipation = BundledDeepSkyParticipation::Disabled;
};

}  // namespace skygate::ui::internal
