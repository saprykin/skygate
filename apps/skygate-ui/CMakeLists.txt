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
    src/SkyObjectSearchModel.cpp
    src/SkyObjectSearchModel.hpp
    src/SkyOverlayLayerSettings.cpp
    src/SkyOverlayLayerSettings.hpp
    src/SkyOverlayLayerVisibility.hpp
    src/SkyHitTargetIndex.cpp
    src/SkyHitTargetIndex.hpp
    src/SkyLogging.cpp
    src/SkyLogging.hpp
    src/SkyObjectTrailBuilder.cpp
    src/SkyObjectTrailBuilder.hpp
    src/SkyCatalogManager.cpp
    src/SkyCatalogManager.hpp
    src/SkySceneComposition.cpp
    src/SkySceneComposition.hpp
    src/SkySceneFramePipeline.cpp
    src/SkySceneFramePipeline.hpp
    src/SkySceneModel.cpp
    src/SkySceneModel.hpp
    src/SkySceneOverlayAdapter.cpp
    src/SkySceneOverlayAdapter.hpp
    src/SkySceneOverlayData.hpp
    src/SkySceneShared.hpp
    src/SkyNightConditionsAdapter.hpp
    src/SkyNightConditionsData.hpp
    src/SkyObjectInspectorBuilder.cpp
    src/SkyObjectInspectorBuilder.hpp
    src/SkyObjectInspectorFormatters.cpp
    src/SkyObjectInspectorFormatters.hpp
    src/SkySelectionOverlayBuilder.cpp
    src/SkySelectionOverlayBuilder.hpp
    src/SkySelectionMarkerBuilder.cpp
    src/SkySelectionMarkerBuilder.hpp
    src/SkyThemeOptionsAdapter.hpp
    src/SkyTrailTargetResolver.cpp
    src/SkyTrailTargetResolver.hpp
    src/SkyConstellationRenderBuilder.cpp
    src/SkyConstellationRenderBuilder.hpp
    src/SkyTheme.cpp
    src/SkyTheme.hpp
    src/SkyTimeController.cpp
    src/SkyTimeController.hpp
    src/TimeZoneAliasCatalog.cpp
    src/TimeZoneAliasCatalog.hpp
    src/TimeZoneCatalogModel.cpp
    src/TimeZoneCatalogModel.hpp
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
    src/SkyContextController.cpp
    src/SkyContextController.hpp
    src/SkyContextControllerCatalog.cpp
    src/SkyContextDomainControllers.cpp
    src/SkyContextDomainControllers.hpp
    src/SkyContextControllerNightConditions.cpp
    src/SkyContextControllerSearch.cpp
    src/SkyContextState.hpp
    src/SkyRenderBodyBuilder.cpp
    src/SkyRenderBodyBuilder.hpp
    src/SkyRenderBuilders.cpp
    src/SkyRenderBuilders.hpp
    src/SkyRenderFrame.hpp
    src/SkyRenderHorizontalLookup.cpp
    src/SkyRenderHorizontalLookup.hpp
    src/SkyRenderLabels.cpp
    src/SkyRenderLabels.hpp
    src/SkyContextControllerSettings.cpp
    src/SkyContextControllerSupport.cpp
    src/SkyContextControllerSupport.hpp
    src/SkyContextControllerTimeline.cpp
    src/SkySettingsSnapshotCodecs.cpp
    src/SkySettingsSnapshotCodecs.hpp
    src/SkySettingsStore.cpp
    src/SkySettingsStore.hpp
    src/SkySettingsValueCodecs.cpp
    src/SkySettingsValueCodecs.hpp
    src/SkyViewportGeometry.cpp
    src/SkyViewportGeometry.hpp
    src/SkyViewportItem.cpp
    src/SkyViewportItem.hpp
    src/SkyViewportSceneGraph.cpp
    src/SkyViewportSceneGraph.hpp
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
        AboutWindow.qml
        Main.qml
        PreferencesActionButton.qml
        PreferencesAppearanceSection.qml
        PreferencesCatalogSection.qml
        PreferencesCheckBox.qml
        PreferencesCityPicker.qml
        PreferencesComboBox.qml
        PreferencesGeneralSection.qml
        PreferencesGroupTitle.qml
        PreferencesSectionButton.qml
        SearchToolbar.qml
        SkyAppMenuButton.qml
        SkyHoverLabel.qml
        SkyInteractionLayer.qml
        SkyObjectInspector.qml
        SkyObjectInspectorButton.qml
        SkyOverlayLabel.qml
        SkyOverlayLayer.qml
        SkySelectionMarker.qml
        PreferencesDraft.qml
        StatusDateTimePopup.qml
        StatusFooter.qml
        StatusNightConditionsPopup.qml
        TimelineToolbar.qml
        PreferencesSkySection.qml
        PreferencesTextField.qml
        PreferencesTimeZonePicker.qml
        PreferencesWindow.qml
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
