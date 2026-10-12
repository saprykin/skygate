#pragma once

#include "SkyObjectTrailInput.hpp"
#include "SkyObjectTrailSampler.hpp"
#include "SkyRenderFrame.hpp"

class SkyObjectTrailBuilder final {
public:
    void appendTrail(SkyRenderFrame& frame, const SkyObjectTrailInput& input) const;

private:
    mutable SkyObjectTrailSampler m_trailSampler;
};
