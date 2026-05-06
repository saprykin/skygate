find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

add_library(skygate-test-support INTERFACE)
add_library(skygate::test-support ALIAS skygate-test-support)

target_link_libraries(skygate-test-support
    INTERFACE
        Qt6::Core
)

target_include_directories(skygate-test-support
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(skygate-test-support INTERFACE cxx_std_20)
