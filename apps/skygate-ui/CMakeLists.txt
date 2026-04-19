find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Network)
find_package(Qt6 6.5 QUIET COMPONENTS Positioning)

qt_standard_project_setup(REQUIRES 6.5)

# Avoid Qt QML CMake policy warnings by opting into modern behavior.
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_library(skygate-ui-lib STATIC
    src/CatalogCoordinator.cpp
    src/CatalogCoordinator.hpp
    src/LocationCatalogModel.cpp
    src/LocationCatalogModel.hpp
    src/SkyCatalogManager.cpp
    src/SkyCatalogManager.hpp
    src/SkySceneModel.cpp
    src/SkySceneModel.hpp
    src/catalog/CatalogDownloadService.cpp
    src/catalog/CatalogDownloadService.hpp
    src/catalog/CatalogPayloadParseService.cpp
    src/catalog/CatalogPayloadParseService.hpp
    src/SkyContextController.cpp
    src/SkyContextController.hpp
    src/SkyContextControllerCatalog.cpp
    src/SkyRenderBuilders.cpp
    src/SkyRenderBuilders.hpp
    src/SkyContextControllerSettings.cpp
    src/SkyContextControllerSupport.cpp
    src/SkyContextControllerSupport.hpp
    src/SkyContextControllerTimeline.cpp
    src/SkySettingsStore.cpp
    src/SkySettingsStore.hpp
    src/SkyViewportItem.cpp
    src/SkyViewportItem.hpp
)

target_include_directories(skygate-ui-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(APPLE)
    set(SKYGATE_LOCATION_USAGE_DESCRIPTION "Skygate uses your location to prefill observer latitude, longitude, and elevation.")
    target_sources(skygate-ui-lib PRIVATE
        src/MacDockIcon.hpp
        src/MacDockIcon.mm
    )
    set_source_files_properties(
        resources/icons/Skygate.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

add_library(skygate::uiapp ALIAS skygate-ui-lib)

qt_add_executable(skygate-ui
    src/main.cpp
)

qt_add_resources(skygateUiIconSources resources/icons.qrc)
target_sources(skygate-ui PRIVATE ${skygateUiIconSources})

qt_add_resources(skygateUiLocationSources resources/locations.qrc)
target_sources(skygate-ui-lib PRIVATE ${skygateUiLocationSources})

if(APPLE)
    target_sources(skygate-ui PRIVATE resources/icons/Skygate.icns)
    set_target_properties(skygate-ui PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "SkyGate"
        MACOSX_BUNDLE_BUNDLE_NAME "SkyGate"
        MACOSX_BUNDLE_EXECUTABLE_NAME "SkyGate"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.skygate.app"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE Skygate
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in
    )
endif()

qt_add_qml_module(skygate-ui
    URI com.skygate.app
    VERSION ${SKYGATE_QML_MODULE_VERSION}
    RESOURCE_PREFIX /qt/qml/
    QML_FILES
        AboutWindow.qml
        Main.qml
        PreferencesActionButton.qml
        PreferencesCatalogSection.qml
        PreferencesCheckBox.qml
        PreferencesCityPicker.qml
        PreferencesComboBox.qml
        PreferencesSectionButton.qml
        SkyInteractionLayer.qml
        SkyOverlayLayer.qml
        SkySettingsDraft.qml
        StatusFooter.qml
        TimelineToolbar.qml
        PreferencesSkySection.qml
        PreferencesTextField.qml
        PreferencesWindow.qml
    RESOURCES
        resources/icons/skygate-icon-512.png
)

target_link_libraries(skygate-ui-lib
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        skygate::ephemeris
)

target_link_libraries(skygate-ui-lib
    PUBLIC
        Qt6::Qml
        Qt6::Quick
)

target_link_libraries(skygate-ui
    PRIVATE
        skygate::uiapp
)

target_compile_definitions(skygate-ui PRIVATE
    SKYGATE_APP_VERSION="${PROJECT_VERSION}"
    SKYGATE_GIT_HASH="${SKYGATE_GIT_HASH}"
)

if(TARGET Qt6::Positioning)
    target_link_libraries(skygate-ui-lib PUBLIC Qt6::Positioning)
    target_compile_definitions(skygate-ui PRIVATE SKYGATE_HAS_POSITIONING=1)
    target_compile_definitions(skygate-ui-lib PRIVATE SKYGATE_HAS_POSITIONING=1)
else()
    target_compile_definitions(skygate-ui PRIVATE SKYGATE_HAS_POSITIONING=0)
    target_compile_definitions(skygate-ui-lib PRIVATE SKYGATE_HAS_POSITIONING=0)
endif()

add_custom_target(run-skygate-ui
    COMMAND $<TARGET_FILE:skygate-ui>
    DEPENDS skygate-ui
    USES_TERMINAL
    COMMENT "Run skygate-ui"
)

if(APPLE)
    add_custom_target(run-skygate-ui-bundle
        COMMAND open $<TARGET_BUNDLE_DIR:skygate-ui>
        DEPENDS skygate-ui
        USES_TERMINAL
        COMMENT "Run skygate-ui bundle"
    )

    unset(skygateMacDeployQtExecutable)
    if(DEFINED MACDEPLOYQT_EXECUTABLE AND EXISTS "${MACDEPLOYQT_EXECUTABLE}")
        set(skygateMacDeployQtExecutable "${MACDEPLOYQT_EXECUTABLE}")
    endif()
    if(NOT skygateMacDeployQtExecutable)
        find_program(skygateMacDeployQtExecutable
            NAMES macdeployqt
            HINTS "${Qt6_DIR}/../../../bin"
        )
    endif()

    if(skygateMacDeployQtExecutable)
        set(skygateDmgStagingDir "${CMAKE_CURRENT_BINARY_DIR}/SkyGate-dmg")
        set(skygateDmgFileName "SkyGate-${PROJECT_VERSION}-${SKYGATE_GIT_HASH}.dmg")
        set(skygateDmgPath "${CMAKE_CURRENT_BINARY_DIR}/${skygateDmgFileName}")

        add_custom_target(package-skygate-ui-dmg
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${skygateDmgStagingDir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${skygateDmgStagingDir}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:skygate-ui>" "${skygateDmgStagingDir}/SkyGate.app"
            COMMAND ${CMAKE_COMMAND} -E create_symlink "/Applications" "${skygateDmgStagingDir}/Applications"
            COMMAND "${skygateMacDeployQtExecutable}" "${skygateDmgStagingDir}/SkyGate.app" -qmldir="${CMAKE_CURRENT_SOURCE_DIR}"
            COMMAND hdiutil create -volname "SkyGate" -srcfolder "${skygateDmgStagingDir}" -ov -format UDZO "${skygateDmgPath}"
            DEPENDS skygate-ui
            USES_TERMINAL
            COMMENT "Package ${skygateDmgFileName} with Applications shortcut"
        )
    else()
        message(WARNING "macdeployqt not found; package-skygate-ui-dmg target is disabled.")
    endif()
endif()
