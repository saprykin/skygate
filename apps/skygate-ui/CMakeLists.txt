find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick)

qt_standard_project_setup(REQUIRES 6.5)

# Avoid Qt QML CMake policy warnings by opting into modern behavior.
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(skygate-ui
    src/main.cpp
)

qt_add_qml_module(skygate-ui
    URI com.skygate.app
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml/
    QML_FILES
        Main.qml
)

target_link_libraries(skygate-ui
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        skygate::ephemeris
)

add_custom_target(run-skygate-ui
    COMMAND $<TARGET_FILE:skygate-ui>
    DEPENDS skygate-ui
    USES_TERMINAL
    COMMENT "Run skygate-ui"
)
