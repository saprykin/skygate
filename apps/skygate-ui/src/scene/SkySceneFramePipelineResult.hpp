#pragma once

#include "EphemerisSnapshot.hpp"
#include "PreparedProjection.hpp"
#include "render/SkyRenderFrame.hpp"

#include <QHash>
#include <QString>

#include <cstdint>

struct SkySceneFramePipelineResult final {
    bool updated = false;
    std::uint64_t snapshotGeneration = 0;
    std::uint64_t renderFrameGeneration = 0;
    const skygate::core::PreparedProjection* preparedProjection = nullptr;
    const skygate::ephemeris::EphemerisSnapshot* snapshot = nullptr;
    const SkyRenderFrame* frame = nullptr;
    const QHash<QString, std::size_t>* stateIndexByBodyId = nullptr;
};
