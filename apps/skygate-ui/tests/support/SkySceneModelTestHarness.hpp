#pragma once

#include "SkyEphemerisTestSupport.hpp"
#include "SkySceneModel.hpp"

#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QtGlobal>

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ui::tests {

struct TestViewport {
    double width = 1100.0;
    double height = 760.0;
};

class SkySceneModelTestHarness final {
public:
    explicit SkySceneModelTestHarness(
        std::vector<ephemeris::CelestialBody> bodies,
        const TestSkyContextConfig& contextConfig = {},
        const TestViewport& viewport = {}
    )
    {
        auto starCatalog = createTestCatalog(std::move(bodies));
        Q_ASSERT(starCatalog != nullptr);
        auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
        Q_ASSERT(ephemerisEngine != nullptr);
        initialize(std::move(starCatalog), std::move(ephemerisEngine), contextConfig, viewport);
    }

    static SkySceneModelTestHarness fromBundledCatalog(
        const TestSkyContextConfig& contextConfig = {},
        const TestViewport& viewport = {}
    )
    {
        auto starCatalog = ephemeris::createBundledStarCatalog();
        Q_ASSERT(starCatalog != nullptr);
        auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
        Q_ASSERT(ephemerisEngine != nullptr);
        return SkySceneModelTestHarness(
            std::move(starCatalog),
            std::move(ephemerisEngine),
            contextConfig,
            viewport
        );
    }

    [[nodiscard]] SkyContextController& controller() noexcept
    {
        return *m_controller;
    }

    [[nodiscard]] const SkyContextController& controller() const noexcept
    {
        return *m_controller;
    }

    [[nodiscard]] SkySceneModel& sceneModel() noexcept
    {
        return m_sceneModel;
    }

    [[nodiscard]] const SkySceneModel& sceneModel() const noexcept
    {
        return m_sceneModel;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return m_controller != nullptr && m_contextConfigured;
    }

    [[nodiscard]] ephemeris::SkySnapshot computeSnapshot() const
    {
        return m_controller->ephemerisEngine()->compute(m_controller->skyContext());
    }

    [[nodiscard]] std::optional<ephemeris::CelestialBodyState> bodyStateById(
        const std::string& bodyId
    ) const
    {
        return findBodyStateById(computeSnapshot(), bodyId);
    }

    [[nodiscard]] bool centerOnBody(const std::string& bodyId)
    {
        const std::optional<ephemeris::CelestialBodyState> state = bodyStateById(bodyId);
        if (!state.has_value()
            || !std::isfinite(state->horizontal.altitudeDeg)
            || !std::isfinite(state->horizontal.azimuthDeg)) {
            return false;
        }

        m_controller->setViewCenter(
            state->horizontal.altitudeDeg,
            state->horizontal.azimuthDeg
        );
        return true;
    }

    [[nodiscard]] const SkyRenderPoint* renderPointForBodyIndex(
        const std::uint32_t bodyIndex
    ) const
    {
        for (const SkyRenderPoint& point : m_sceneModel.renderPointSpan()) {
            if (point.bodyIndex == bodyIndex) {
                return &point;
            }
        }

        return nullptr;
    }

    [[nodiscard]] const SkyRenderPoint* renderPointForBodyId(const std::string& bodyId) const
    {
        const std::optional<ephemeris::CelestialBodyState> state = bodyStateById(bodyId);
        if (!state.has_value()) {
            return nullptr;
        }

        return renderPointForBodyIndex(state->bodyIndex);
    }

    [[nodiscard]] std::optional<SkyRenderGlyph> renderGlyphWithLabel(const QString& label) const
    {
        for (const SkyRenderGlyph& glyph : m_sceneModel.renderGlyphSpan()) {
            if (m_sceneModel.objectLabelAt(glyph.x, glyph.y) == label) {
                return glyph;
            }
        }

        return std::nullopt;
    }

private:
    SkySceneModelTestHarness(
        std::unique_ptr<ephemeris::IStarCatalog> starCatalog,
        std::unique_ptr<ephemeris::IEphemerisEngine> ephemerisEngine,
        const TestSkyContextConfig& contextConfig,
        const TestViewport& viewport
    )
    {
        initialize(std::move(starCatalog), std::move(ephemerisEngine), contextConfig, viewport);
    }

    void initialize(
        std::unique_ptr<ephemeris::IStarCatalog> starCatalog,
        std::unique_ptr<ephemeris::IEphemerisEngine> ephemerisEngine,
        const TestSkyContextConfig& contextConfig,
        const TestViewport& viewport
    )
    {
        m_controller = createTestController(std::move(starCatalog), std::move(ephemerisEngine));
        Q_ASSERT(m_controller != nullptr);
        m_contextConfigured = configureTestSkyContext(*m_controller, contextConfig);
        m_sceneModel.setSkyContextController(m_controller.get());
        m_sceneModel.setViewportSize(viewport.width, viewport.height);
    }

    std::unique_ptr<SkyContextController> m_controller;
    SkySceneModel m_sceneModel;
    bool m_contextConfigured = false;
};

}  // namespace skygate::ui::tests
