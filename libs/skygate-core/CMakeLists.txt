add_library(skygate-core STATIC
    include/skygate/core/IProjection.hpp
    include/skygate/core/PreparedProjection.hpp
    include/skygate/core/ITimeSource.hpp
    include/skygate/core/ProjectionFactory.hpp
    include/skygate/core/ProjectionTypes.hpp
    include/skygate/core/SkyTypes.hpp
    include/skygate/core/SystemTimeSource.hpp
    include/skygate/core/Types.hpp
    include/skygate/core/math/AngleMath.hpp
    include/skygate/core/math/Geometry2d.hpp
    include/skygate/core/math/GeometryMath.hpp
    include/skygate/core/math/LinePattern.hpp
    include/skygate/core/math/MathConstants.hpp
    include/skygate/core/math/ProjectionMath.hpp
    include/skygate/core/math/ProjectedPolylineBuilder.hpp
    include/skygate/core/math/ScreenGeometry.hpp
    include/skygate/core/math/SpatialIndex2d.hpp
    include/skygate/core/math/SphericalGeometry.hpp
    include/skygate/core/math/ViewportMath.hpp
    include/skygate/core/projections/AzimuthalEquidistantProjection.hpp
    include/skygate/core/projections/PerspectiveProjection.hpp
    include/skygate/core/projections/StereographicProjection.hpp
    src/SkyTypes.cpp
    src/PreparedProjection.cpp
    src/ProjectionTypes.cpp
    src/SystemTimeSource.cpp
    src/math/AngleMath.cpp
    src/math/Geometry2d.cpp
    src/math/GeometryMath.cpp
    src/math/LinePattern.cpp
    src/math/ProjectionMath.cpp
    src/math/ProjectedPolylineBuilder.cpp
    src/math/SpatialIndex2d.cpp
    src/math/SphericalGeometry.cpp
    src/math/ViewportMath.cpp
    src/ProjectionFactory.cpp
    src/projections/ProjectionAlgorithms.cpp
    src/projections/ProjectionAlgorithms.hpp
    src/projections/AzimuthalEquidistantProjection.cpp
    src/projections/PerspectiveProjection.cpp
    src/projections/ProjectionPipeline.cpp
    src/projections/StereographicProjection.cpp
)

target_include_directories(skygate-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(skygate-core PUBLIC cxx_std_20)

add_library(skygate::core ALIAS skygate-core)
