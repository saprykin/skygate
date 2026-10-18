#pragma once

#include "ProjectionParams.hpp"
#include "ProjectionType.hpp"
#include "math/Vector3d.hpp"

namespace skygate::core {

struct ProjectionFrame final {
    ProjectionType projectionType = ProjectionType::Stereographic;
    ProjectionParams params;
    Vector3d center{};
    Vector3d right{};
    Vector3d up{};
    double circularMaxRadius = 0.0;
    double rectHalfWidth = 0.0;
    double rectHalfHeight = 0.0;
};

}  // namespace skygate::core
