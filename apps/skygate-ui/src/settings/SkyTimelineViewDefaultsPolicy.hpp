#pragma once

namespace skygate::ui::internal {

// Shared defaults for the timeline and view domains. Runtime state and
// persisted settings snapshots initialize from these values.
class SkyTimelineViewDefaultsPolicy final {
public:
    static constexpr int kDefaultStepSeconds = 60;
    static constexpr double kDefaultMagnitudeCutoff = 6.0;
    static constexpr double kDefaultFieldOfViewDeg = 100.0;
    // A persisted view center without saved keys restores at the horizon
    // facing north, while a fresh runtime view starts from
    // skygate::core::ViewportMath's 45/180 defaults.
    static constexpr double kLegacyViewCenterAltitudeDeg = 0.0;
    static constexpr double kLegacyViewCenterAzimuthDeg = 0.0;
};

}  // namespace skygate::ui::internal
