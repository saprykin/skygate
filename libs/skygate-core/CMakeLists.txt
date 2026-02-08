add_library(skygate-core STATIC
    include/skygate/core/IProjection.hpp
    include/skygate/core/ITimeSource.hpp
    include/skygate/core/ProjectionFactory.hpp
    include/skygate/core/Types.hpp
    include/skygate/core/math/MathConstants.hpp
    include/skygate/core/math/ProjectionMath.hpp
    include/skygate/core/projections/PerspectiveProjection.hpp
    include/skygate/core/projections/StereographicProjection.hpp
    src/GeoLocation.cpp
    src/math/ProjectionMath.cpp
    src/ProjectionFactory.cpp
    src/projections/PerspectiveProjection.cpp
    src/projections/StereographicProjection.cpp
)

target_include_directories(skygate-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(skygate-core PUBLIC cxx_std_20)

add_library(skygate::core ALIAS skygate-core)
