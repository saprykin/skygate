#pragma once

#include "ephemeris/SkyEphemerisSession.hpp"

#include <QVariantList>

namespace skygate::ephemeris {
class IEphemerisEngine;
}

// Builds the degradation reasons shown for the current ephemeris request and
// remembers the last published list so overlay-only rebuilds can tell whether
// the text changed.
class SkySceneDegradationReporter final {
public:
    [[nodiscard]] QVariantList reasons() const;
    [[nodiscard]] bool hasReasons() const;
    [[nodiscard]] bool
    update(const SkyEphemerisSession::RequestContext& context, const skygate::ephemeris::IEphemerisEngine* engine);
    void clear();

private:
    QVariantList m_reasons;
};
