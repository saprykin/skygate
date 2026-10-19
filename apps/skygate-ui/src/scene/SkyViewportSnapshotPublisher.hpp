#pragma once

#include "SkyViewportSnapshot.hpp"
#include "SkyViewportSnapshotInput.hpp"

#include <cstdint>
#include <memory>
#include <optional>

// Owns the immutable snapshot handed to the viewport. A replacement is built
// only when the render frame generation or the active trail target changes;
// the snapshot generation identifies each handoff and stays independent of the
// render frame generation that produced it.
class SkyViewportSnapshotPublisher final {
public:
    [[nodiscard]] std::shared_ptr<const SkyViewportSnapshot> snapshot() const noexcept;
    [[nodiscard]] bool publish(const SkyViewportSnapshotInput& input);
    [[nodiscard]] bool clear();
    void invalidatePublishedFrame();

private:
    std::shared_ptr<const SkyViewportSnapshot> m_snapshot;
    std::uint64_t m_snapshotGeneration = 0;
    std::uint64_t m_renderFrameGeneration = 0;
    std::optional<std::uint32_t> m_trailTargetBodyIndex;
};
