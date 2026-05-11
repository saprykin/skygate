find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Network)
find_package(Qt6 6.5 QUIET COMPONENTS Positioning)

qt_standard_project_setup(REQUIRES 6.5)

# Avoid Qt QML CMake policy warnings by opting into modern behavior.
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_library(skygate-ui-lib STATIC
    src/catalog/CatalogCoordinator.cpp
    src/catalog/CatalogCoordinator.hpp
    src/location/LocationCatalogModel.cpp
    src/location/LocationCatalogModel.hpp
    src/search/SkyObjectSearchModel.cpp
    src/search/SkyObjectSearchModel.hpp
    src/theme/SkyOverlayLayerSettings.cpp
    src/theme/SkyOverlayLayerSettings.hpp
    src/theme/SkyOverlayLayerVisibility.hpp
    src/selection/SkyHitTargetIndex.cpp
    src/selection/SkyHitTargetIndex.hpp
    src/diagnostics/SkyLogging.cpp
    src/diagnostics/SkyLogging.hpp
    src/selection/SkyObjectTrailBuilder.cpp
    src/selection/SkyObjectTrailBuilder.hpp
    src/catalog/SkyCatalogManager.cpp
    src/catalog/SkyCatalogManager.hpp
    src/scene/SkySceneComposition.cpp
    src/scene/SkySceneComposition.hpp
    src/scene/SkySceneFramePipeline.cpp
    src/scene/SkySceneFramePipeline.hpp
    src/scene/SkySceneModel.cpp
    src/scene/SkySceneModel.hpp
    src/scene/SkySceneOverlayAdapter.cpp
    src/scene/SkySceneOverlayAdapter.hpp
    src/scene/SkySceneOverlayData.hpp
    src/scene/SkySceneShared.hpp
    src/app/SkyNightConditionsAdapter.hpp
    src/app/SkyNightConditionsData.hpp
    src/selection/SkyObjectInspectorBuilder.cpp
    src/selection/SkyObjectInspectorBuilder.hpp
    src/selection/SkyObjectInspectorFormatters.cpp
    src/selection/SkyObjectInspectorFormatters.hpp
    src/selection/SkySelectionOverlayBuilder.cpp
    src/selection/SkySelectionOverlayBuilder.hpp
    src/selection/SkySelectionMarkerBuilder.cpp
    src/selection/SkySelectionMarkerBuilder.hpp
    src/theme/SkyThemeOptionsAdapter.hpp
    src/selection/SkyTrailTargetResolver.cpp
    src/selection/SkyTrailTargetResolver.hpp
    src/render/SkyConstellationRenderBuilder.cpp
    src/render/SkyConstellationRenderBuilder.hpp
    src/theme/SkyTheme.cpp
    src/theme/SkyTheme.hpp
    src/time/SkyTimeController.cpp
    src/time/SkyTimeController.hpp
    src/time/TimeZoneAliasCatalog.cpp
    src/time/TimeZoneAliasCatalog.hpp
    src/time/TimeZoneCatalogModel.cpp
    src/time/TimeZoneCatalogModel.hpp
    src/catalog/SkyActiveCatalogBuilder.cpp
    src/catalog/SkyActiveCatalogBuilder.hpp
    src/catalog/SkyCatalogCacheController.cpp
    src/catalog/SkyCatalogCacheController.hpp
    src/catalog/SkyCatalogConstellationStore.cpp
    src/catalog/SkyCatalogConstellationStore.hpp
    src/catalog/SkyCatalogRuntime.cpp
    src/catalog/SkyCatalogRuntime.hpp
    src/catalog/CatalogDownloadService.cpp
    src/catalog/CatalogDownloadService.hpp
    src/catalog/SkyCatalogImportWorkflow.cpp
    src/catalog/SkyCatalogImportWorkflow.hpp
    src/catalog/SkyCatalogPresets.cpp
    src/catalog/SkyCatalogPresets.hpp
    src/catalog/SkyCatalogText.cpp
    src/catalog/SkyCatalogText.hpp
    src/catalog/CatalogPayloadParseService.cpp
    src/catalog/CatalogPayloadParseService.hpp
    src/app/SkyContextController.cpp
    src/app/SkyContextController.hpp
    src/app/SkyContextControllerCatalog.cpp
    src/app/SkyContextDomainControllers.cpp
    src/app/SkyContextDomainControllers.hpp
    src/app/SkyContextControllerNightConditions.cpp
    src/app/SkyContextControllerSearch.cpp
    src/app/SkyContextState.hpp
    src/render/SkyRenderBodyBuilder.cpp
    src/render/SkyRenderBodyBuilder.hpp
    src/render/SkyRenderBuilders.cpp
    src/render/SkyRenderBuilders.hpp
    src/render/SkyRenderFrame.hpp
    src/render/SkyRenderHorizontalLookup.cpp
    src/render/SkyRenderHorizontalLookup.hpp
    src/render/SkyRenderLabels.cpp
    src/render/SkyRenderLabels.hpp
    src/app/SkyContextControllerSettings.cpp
    src/app/SkyContextControllerSupport.cpp
    src/app/SkyContextControllerSupport.hpp
    src/app/SkyContextControllerTimeline.cpp
    src/settings/SkySettingsSnapshotCodecs.cpp
    src/settings/SkySettingsSnapshotCodecs.hpp
    src/settings/SkySettingsStore.cpp
    src/settings/SkySettingsStore.hpp
    src/settings/SkySettingsValueCodecs.cpp
    src/settings/SkySettingsValueCodecs.hpp
    src/render/SkyViewportGeometry.cpp
    src/render/SkyViewportGeometry.hpp
    src/render/SkyViewportItem.cpp
    src/render/SkyViewportItem.hpp
    src/render/SkyViewportSceneGraph.cpp
    src/render/SkyViewportSceneGraph.hpp
)

target_include_directories(skygate-ui-lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app
        ${CMAKE_CURRENT_SOURCE_DIR}/src/catalog
        ${CMAKE_CURRENT_SOURCE_DIR}/src/diagnostics
        ${CMAKE_CURRENT_SOURCE_DIR}/src/location
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene
        ${CMAKE_CURRENT_SOURCE_DIR}/src/search
        ${CMAKE_CURRENT_SOURCE_DIR}/src/selection
        ${CMAKE_CURRENT_SOURCE_DIR}/src/settings
        ${CMAKE_CURRENT_SOURCE_DIR}/src/theme
        ${CMAKE_CURRENT_SOURCE_DIR}/src/time
)

if(APPLE)
    set(SKYGATE_LOCATION_USAGE_DESCRIPTION "Skygate uses your location to prefill observer latitude, longitude, and elevation.")
    target_sources(skygate-ui-lib PRIVATE
        src/platform/MacDockIcon.hpp
        src/platform/MacDockIcon.mm
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

qt_add_resources(skygateUiThemeSources resources/themes.qrc)
target_sources(skygate-ui-lib PRIVATE ${skygateUiThemeSources})

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

if(WIN32)
    target_sources(skygate-ui PRIVATE windows/SkyGate.rc)
    set_target_properties(skygate-ui PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

qt_add_qml_module(skygate-ui
    URI com.skygate.app
    VERSION ${SKYGATE_QML_MODULE_VERSION}
    RESOURCE_PREFIX /qt/qml/
    QML_FILES
        qml/Main.qml
        qml/windows/AboutWindow.qml
        qml/windows/PreferencesWindow.qml
        qml/preferences/PreferencesDraft.qml
        qml/preferences/PreferencesGeneralSection.qml
        qml/preferences/PreferencesSkySection.qml
        qml/preferences/PreferencesAppearanceSection.qml
        qml/preferences/PreferencesCatalogSection.qml
        qml/preferences/PreferencesCityPicker.qml
        qml/preferences/PreferencesTimeZonePicker.qml
        qml/controls/PreferencesActionButton.qml
        qml/controls/PreferencesCheckBox.qml
        qml/controls/PreferencesComboBox.qml
        qml/controls/PreferencesGroupTitle.qml
        qml/controls/PreferencesSectionButton.qml
        qml/controls/PreferencesTextField.qml
        qml/sky/SkyHoverLabel.qml
        qml/sky/SkyInteractionLayer.qml
        qml/sky/SkyObjectInspector.qml
        qml/sky/SkyObjectInspectorButton.qml
        qml/sky/SkyOverlayLabel.qml
        qml/sky/SkyOverlayLayer.qml
        qml/sky/SkySelectionMarker.qml
        qml/toolbars/SearchToolbar.qml
        qml/toolbars/SkyAppMenuButton.qml
        qml/toolbars/TimelineToolbar.qml
        qml/status/StatusDateTimePopup.qml
        qml/status/StatusFooter.qml
        qml/status/StatusNightConditionsPopup.qml
    RESOURCES
        resources/icons/skygate-icon-512.png
        resources/icons/skygate-about-icon-512.png
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
    target_compile_definitions(skygate-ui-lib PUBLIC SKYGATE_HAS_POSITIONING=1)
else()
    target_compile_definitions(skygate-ui PRIVATE SKYGATE_HAS_POSITIONING=0)
    target_compile_definitions(skygate-ui-lib PUBLIC SKYGATE_HAS_POSITIONING=0)
endif()

add_custom_target(run-skygate-ui
    COMMAND $<TARGET_FILE:skygate-ui>
    DEPENDS skygate-ui
    USES_TERMINAL
    COMMENT "Run skygate-ui"
)

unset(skygateMacDeployQtExecutable)
if(APPLE)
    if(DEFINED MACDEPLOYQT_EXECUTABLE AND EXISTS "${MACDEPLOYQT_EXECUTABLE}")
        set(skygateMacDeployQtExecutable "${MACDEPLOYQT_EXECUTABLE}")
    endif()
    if(NOT skygateMacDeployQtExecutable)
        find_program(skygateMacDeployQtExecutable
            NAMES macdeployqt
            HINTS "${Qt6_DIR}/../../../bin"
        )
    endif()
endif()

unset(skygateWinDeployQtExecutable)
if(WIN32)
    if(DEFINED WINDEPLOYQT_EXECUTABLE AND EXISTS "${WINDEPLOYQT_EXECUTABLE}")
        set(skygateWinDeployQtExecutable "${WINDEPLOYQT_EXECUTABLE}")
    endif()
    if(NOT skygateWinDeployQtExecutable)
        find_program(skygateWinDeployQtExecutable
            NAMES windeployqt
            HINTS "${Qt6_DIR}/../../../bin"
        )
    endif()
endif()

install(TARGETS skygate-ui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES linux/com.skygate.SkyGate.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )

    foreach(skygateIconSize IN ITEMS 16 32 64 128 256 512 1024)
        install(FILES resources/icons/skygate-icon-${skygateIconSize}.png
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${skygateIconSize}x${skygateIconSize}/apps
            RENAME skygate.png
        )
    endforeach()
endif()

if(APPLE AND skygateMacDeployQtExecutable)
    install(CODE "
        set(skygateInstalledBundle \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/$<TARGET_FILE_NAME:skygate-ui>.app\")
        execute_process(
            COMMAND \"${skygateMacDeployQtExecutable}\" \"\${skygateInstalledBundle}\" \"-qmldir=${CMAKE_CURRENT_SOURCE_DIR}\" -appstore-compliant
            RESULT_VARIABLE skygateDeployResult
        )
        if(NOT skygateDeployResult EQUAL 0)
            message(FATAL_ERROR \"macdeployqt failed for \${skygateInstalledBundle}.\")
        endif()
    ")
elseif(APPLE)
    message(WARNING "macdeployqt not found; install/package targets will not deploy Qt runtime files.")
endif()

if(WIN32 AND skygateWinDeployQtExecutable)
    install(CODE "
        set(skygateInstalledExecutable \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:skygate-ui>\")
        execute_process(
            COMMAND \"${skygateWinDeployQtExecutable}\" --qmldir \"${CMAKE_CURRENT_SOURCE_DIR}\" \"\${skygateInstalledExecutable}\"
            RESULT_VARIABLE skygateDeployResult
        )
        if(NOT skygateDeployResult EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed for \${skygateInstalledExecutable}.\")
        endif()
    ")
elseif(WIN32)
    message(WARNING "windeployqt not found; install/package targets will not deploy Qt runtime files.")
endif()

if(WIN32)
    set(skygateVcpkgRuntimeDllDirectories)
    set(skygateVcpkgInstalledDir)
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(skygateVcpkgInstalledDir "${VCPKG_INSTALLED_DIR}")
    elseif(DEFINED _VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(skygateVcpkgInstalledDir "${_VCPKG_INSTALLED_DIR}")
    elseif(DEFINED VCPKG_TARGET_TRIPLET)
        set(skygateVcpkgInstalledDir "${CMAKE_BINARY_DIR}/vcpkg_installed")
    endif()
    if(skygateVcpkgInstalledDir AND DEFINED VCPKG_TARGET_TRIPLET)
        list(APPEND skygateVcpkgRuntimeDllDirectories
            "${skygateVcpkgInstalledDir}/${VCPKG_TARGET_TRIPLET}/bin"
            "$<$<CONFIG:Debug>:${skygateVcpkgInstalledDir}/${VCPKG_TARGET_TRIPLET}/debug/bin>"
        )
    endif()

    set(skygateRuntimeDllCopyScript
        "${CMAKE_CURRENT_BINARY_DIR}/copy-skygate-ui-runtime-dlls-$<CONFIG>.cmake"
    )
    string(CONCAT skygateRuntimeDllCopyScriptContent
        "set(SKYGATE_RUNTIME_DLLS \"$<TARGET_RUNTIME_DLLS:skygate-ui>\")\n"
        "set(SKYGATE_RUNTIME_DLL_DIRECTORIES \"${skygateVcpkgRuntimeDllDirectories}\")\n"
        "set(SKYGATE_RUNTIME_OUTPUT_DIR \"$<TARGET_FILE_DIR:skygate-ui>\")\n"
        "include(\"${CMAKE_SOURCE_DIR}/cmake/SkyGateCopyRuntimeDlls.cmake\")\n"
    )
    file(GENERATE
        OUTPUT "${skygateRuntimeDllCopyScript}"
        CONTENT "${skygateRuntimeDllCopyScriptContent}"
    )
    add_custom_command(TARGET skygate-ui POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
                -P "${skygateRuntimeDllCopyScript}"
        COMMENT "Copy SkyGate runtime DLLs"
    )

    set(skygateRuntimeDllInstallScript
        "${CMAKE_CURRENT_BINARY_DIR}/install-skygate-ui-runtime-dlls-$<CONFIG>.cmake"
    )
    string(CONCAT skygateRuntimeDllInstallScriptContent
        "set(SKYGATE_RUNTIME_DLLS \"$<TARGET_RUNTIME_DLLS:skygate-ui>\")\n"
        "set(SKYGATE_RUNTIME_DLL_DIRECTORIES \"${skygateVcpkgRuntimeDllDirectories}\")\n"
        "set(SKYGATE_RUNTIME_OUTPUT_DIR \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")\n"
        "include(\"${CMAKE_SOURCE_DIR}/cmake/SkyGateCopyRuntimeDlls.cmake\")\n"
    )
    file(GENERATE
        OUTPUT "${skygateRuntimeDllInstallScript}"
        CONTENT "${skygateRuntimeDllInstallScriptContent}"
    )
    install(SCRIPT "${skygateRuntimeDllInstallScript}")
endif()

if(APPLE)
    add_custom_target(run-skygate-ui-bundle
        COMMAND open $<TARGET_BUNDLE_DIR:skygate-ui>
        DEPENDS skygate-ui
        USES_TERMINAL
        COMMENT "Run skygate-ui bundle"
    )

    if(skygateMacDeployQtExecutable)
        set(skygateDmgStagingDir "${CMAKE_CURRENT_BINARY_DIR}/SkyGate-dmg")
        set(skygateDmgFileName "SkyGate-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${skygatePackageArchitecture}-${SKYGATE_GIT_HASH}.dmg")
        set(skygateDmgPath "${CMAKE_CURRENT_BINARY_DIR}/${skygateDmgFileName}")

        add_custom_target(package-skygate-ui-dmg
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${skygateDmgStagingDir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${skygateDmgStagingDir}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:skygate-ui>" "${skygateDmgStagingDir}/SkyGate.app"
            COMMAND ${CMAKE_COMMAND} -E create_symlink "/Applications" "${skygateDmgStagingDir}/Applications"
            COMMAND "${skygateMacDeployQtExecutable}" "${skygateDmgStagingDir}/SkyGate.app" -qmldir="${CMAKE_CURRENT_SOURCE_DIR}" -appstore-compliant
            COMMAND hdiutil create -volname "SkyGate" -srcfolder "${skygateDmgStagingDir}" -ov -format UDZO "${skygateDmgPath}"
            DEPENDS skygate-ui
            USES_TERMINAL
            COMMENT "Package ${skygateDmgFileName} with Applications shortcut"
        )
    else()
        message(WARNING "macdeployqt not found; package-skygate-ui-dmg target is disabled.")
    endif()
endif()
