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
    src/catalog/CatalogParseOptions.cpp
    src/catalog/CatalogParseOptions.hpp
    src/location/ILocationPermissionGate.hpp
    src/location/LocationCatalogModel.cpp
    src/location/LocationCatalogModel.hpp
    src/location/SkyDeviceLocationAcquisition.cpp
    src/location/SkyDeviceLocationAcquisition.hpp
    src/location/SkyLocationPermissionGate.cpp
    src/location/SkyLocationPermissionGate.hpp
    src/location/SkyLocationStatusText.cpp
    src/location/SkyLocationStatusText.hpp
    src/search/SkyObjectSearchModel.cpp
    src/search/SkyObjectSearchModel.hpp
    src/theme/SkyOverlayLayerSettings.cpp
    src/theme/SkyOverlayLayerSettings.hpp
    src/theme/SkyOverlayLayerVisibility.hpp
    src/selection/SkyHitTargetIndex.cpp
    src/selection/SkyHitTargetIndex.hpp
    src/diagnostics/SkyLogging.cpp
    src/diagnostics/SkyLogging.hpp
    src/diagnostics/SkyLoggingConfiguration.hpp
    src/diagnostics/SkyPerformanceLogging.cpp
    src/diagnostics/SkyPerformanceLogging.hpp
    src/ephemeris/SkyEphemerisAssetStatus.cpp
    src/ephemeris/SkyEphemerisAssetStatus.hpp
    src/ephemeris/SkyEphemerisCacheController.cpp
    src/ephemeris/SkyEphemerisCacheController.hpp
    src/ephemeris/SkyEphemerisDataCacheSnapshot.cpp
    src/ephemeris/SkyEphemerisDataCacheSnapshot.hpp
    src/ephemeris/SkyEphemerisDataCacheSnapshotPolicy.cpp
    src/ephemeris/SkyEphemerisDataCacheSnapshotPolicy.hpp
    src/ephemeris/SkyEphemerisDataManager.cpp
    src/ephemeris/SkyEphemerisDataManager.hpp
    src/ephemeris/SkyEphemerisDataProfileIds.hpp
    src/ephemeris/SkyEphemerisDownloadService.cpp
    src/ephemeris/SkyEphemerisDownloadService.hpp
    src/ephemeris/SkyEphemerisManifestLoader.cpp
    src/ephemeris/SkyEphemerisManifestLoader.hpp
    src/ephemeris/SkyEphemerisPathSegmentPolicy.cpp
    src/ephemeris/SkyEphemerisPathSegmentPolicy.hpp
    src/ephemeris/SkyEphemerisSession.cpp
    src/ephemeris/SkyEphemerisSession.hpp
    src/ephemeris/SkyEphemerisSettingsPolicy.cpp
    src/ephemeris/SkyEphemerisSettingsPolicy.hpp
    src/ephemeris/SkyEphemerisSnapshotAdapter.cpp
    src/ephemeris/SkyEphemerisSnapshotAdapter.hpp
    src/ephemeris/SkyEphemerisStatusText.cpp
    src/ephemeris/SkyEphemerisStatusText.hpp
    src/ephemeris/SkyEphemerisUpdateOperation.hpp
    src/ephemeris/SkyEphemerisUpdateWorkflow.cpp
    src/ephemeris/SkyEphemerisUpdateWorkflow.hpp
    src/selection/SkyObjectTrailAlignment.cpp
    src/selection/SkyObjectTrailAlignment.hpp
    src/selection/SkyObjectTrailBuilder.cpp
    src/selection/SkyObjectTrailBuilder.hpp
    src/selection/SkyObjectTrailInput.hpp
    src/selection/SkyObjectTrailInterpolator.cpp
    src/selection/SkyObjectTrailInterpolator.hpp
    src/selection/SkyObjectTrailSampler.cpp
    src/selection/SkyObjectTrailSampler.hpp
    src/selection/SkyObjectTrailSamplingInput.hpp
    src/catalog/SkyCatalogManager.cpp
    src/catalog/SkyCatalogManager.hpp
    src/scene/SkyInspectorField.hpp
    src/scene/SkyInspectorPlacement.hpp
    src/scene/SkyOverlayItem.hpp
    src/scene/SkySceneComposer.cpp
    src/scene/SkySceneComposer.hpp
    src/scene/SkySceneCompositionInput.hpp
    src/scene/SkySceneCompositionResult.hpp
    src/scene/SkySceneDegradationReporter.cpp
    src/scene/SkySceneDegradationReporter.hpp
    src/scene/SkySceneFrameData.hpp
    src/scene/SkySceneFramePipeline.cpp
    src/scene/SkySceneFramePipeline.hpp
    src/scene/SkySceneFramePipelineInput.hpp
    src/scene/SkySceneFramePipelineResult.hpp
    src/scene/SkySceneModel.cpp
    src/scene/SkySceneModel.hpp
    src/scene/SkySceneOverlayAdapter.cpp
    src/scene/SkySceneOverlayAdapter.hpp
    src/scene/SkySceneLookupKey.hpp
    src/scene/SkySelectedObjectInspector.hpp
    src/scene/SkySelectionMarker.hpp
    src/scene/SkyViewportSnapshot.hpp
    src/scene/SkyViewportSnapshotInput.hpp
    src/scene/SkyViewportSnapshotPublisher.cpp
    src/scene/SkyViewportSnapshotPublisher.hpp
    src/app/SkyNightConditionsAdapter.hpp
    src/app/SkyNightConditionsData.hpp
    src/app/SkyNightConditionSunRow.hpp
    src/selection/SkyObjectInspectorBuilder.cpp
    src/selection/SkyObjectInspectorBuilder.hpp
    src/selection/SkyObjectInspectorFormatters.cpp
    src/selection/SkyObjectInspectorFormatters.hpp
    src/selection/SkySelectionOverlayBuilder.cpp
    src/selection/SkySelectionOverlayBuilder.hpp
    src/selection/SkySelectionOverlayInput.hpp
    src/selection/SkySelectionMarkerBuilder.cpp
    src/selection/SkySelectionMarkerBuilder.hpp
    src/theme/SkyThemeOptionsAdapter.hpp
    src/selection/SkyTrailTargetResolver.cpp
    src/selection/SkyTrailTargetResolver.hpp
    src/render/SkyConstellationRenderBuilder.cpp
    src/render/SkyConstellationRenderBuilder.hpp
    src/theme/SkyThemeDefinition.cpp
    src/theme/SkyThemeDefinition.hpp
    src/theme/SkyThemeDescriptor.hpp
    src/theme/SkyThemeJsonCodec.cpp
    src/theme/SkyThemeJsonCodec.hpp
    src/theme/SkyThemeOption.hpp
    src/theme/SkyThemePalette.cpp
    src/theme/SkyThemePalette.hpp
    src/theme/SkyThemeRenderPalette.hpp
    src/theme/SkyThemeRepository.cpp
    src/theme/SkyThemeRepository.hpp
    src/theme/SkyThemeUiPalette.hpp
    src/time/SkyTimeController.cpp
    src/time/SkyTimeController.hpp
    src/time/SkyLiveClock.cpp
    src/time/SkyLiveClock.hpp
    src/time/SkyQtTimeCodec.cpp
    src/time/SkyQtTimeCodec.hpp
    src/time/TimeZoneAliasCatalog.cpp
    src/time/TimeZoneAliasCatalog.hpp
    src/time/TimeZoneCatalogModel.cpp
    src/time/TimeZoneCatalogModel.hpp
    src/time/TimeZoneCatalogEntry.hpp
    src/catalog/SkyCatalogCacheController.cpp
    src/catalog/SkyCatalogCacheController.hpp
    src/catalog/SkyCatalogCollectionCacheLoadResult.hpp
    src/catalog/SkyCatalogCollectionCacheSnapshot.hpp
    src/catalog/SkyCatalogCollectionManifestStore.cpp
    src/catalog/SkyCatalogCollectionManifestStore.hpp
    src/catalog/SkyCatalogCollectionPersistRequest.hpp
    src/catalog/SkyCatalogCollectionRestoreResult.hpp
    src/catalog/SkyCatalogCollectionStore.cpp
    src/catalog/SkyCatalogCollectionStore.hpp
    src/catalog/SkyCatalogLegacyCacheSnapshot.hpp
    src/catalog/SkyCatalogLegacySlot.cpp
    src/catalog/SkyCatalogLegacySlot.hpp
    src/catalog/SkyCatalogSourceCachePaths.cpp
    src/catalog/SkyCatalogSourceCachePaths.hpp
    src/catalog/SkyCatalogSourceCacheRecord.cpp
    src/catalog/SkyCatalogSourceCacheRecord.hpp
    src/catalog/SkyCatalogSourceRecordCodec.cpp
    src/catalog/SkyCatalogSourceRecordCodec.hpp
    src/catalog/SkyCatalogConstellationStore.cpp
    src/catalog/SkyCatalogConstellationStore.hpp
    src/catalog/SkyCatalogRuntime.cpp
    src/catalog/SkyCatalogRuntime.hpp
    src/catalog/SkyCatalogRuntimeBuildOptions.hpp
    src/catalog/SkyCatalogRuntimeResult.hpp
    src/catalog/SkyCatalogSourceRecord.cpp
    src/catalog/SkyCatalogSourceRecord.hpp
    src/catalog/CatalogDownloadCancellation.cpp
    src/catalog/CatalogDownloadCancellation.hpp
    src/catalog/CatalogDownloadService.cpp
    src/catalog/CatalogDownloadService.hpp
    src/catalog/SkyCatalogImportStatus.hpp
    src/catalog/SkyCatalogImportWorkflow.cpp
    src/catalog/SkyCatalogImportWorkflow.hpp
    src/catalog/SkyCatalogSourceImportResult.hpp
    src/catalog/SkyCatalogSourceOperation.hpp
    src/catalog/SkyCatalogSourceOperationRegistry.cpp
    src/catalog/SkyCatalogSourceOperationRegistry.hpp
    src/catalog/SkyCatalogSourcePersistEntry.hpp
    src/catalog/SkyCatalogSourceRestoreEntry.hpp
    src/catalog/SkyConstellationLineImportResult.cpp
    src/catalog/SkyConstellationLineImportResult.hpp
    src/catalog/SkyCatalogPresets.cpp
    src/catalog/SkyCatalogPresets.hpp
    src/catalog/SkyCatalogSourceDescriptor.cpp
    src/catalog/SkyCatalogSourceDescriptor.hpp
    src/catalog/SkyCatalogSourceInstance.cpp
    src/catalog/SkyCatalogSourceInstance.hpp
    src/catalog/SkyCatalogSourceIds.hpp
    src/catalog/SkyCatalogSourceCollectionModel.cpp
    src/catalog/SkyCatalogSourceCollectionModel.hpp
    src/catalog/SkyCatalogSourcePresetModel.cpp
    src/catalog/SkyCatalogSourcePresetModel.hpp
    src/catalog/SkyCatalogText.cpp
    src/catalog/SkyCatalogText.hpp
    src/catalog/SkyPayloadView.hpp
    src/catalog/CatalogPayloadParseService.cpp
    src/catalog/CatalogPayloadParseService.hpp
    src/app/SkyContextController.cpp
    src/app/SkyContextController.hpp
    src/catalog/SkyCatalogRelatedDataCodec.cpp
    src/catalog/SkyCatalogRelatedDataCodec.hpp
    src/app/SkyContextControllerCatalog.cpp
    src/app/SkyContextControllerLocation.cpp
    src/app/SkyContextControllerNightConditions.cpp
    src/app/SkyContextControllerSearch.cpp
    src/app/SkyContextControllerSettings.cpp
    src/app/SkyContextControllerTimeline.cpp
    src/app/SkyContextLocationSource.hpp
    src/app/SkyContextLocationSourceCodec.cpp
    src/app/SkyContextLocationSourceCodec.hpp
    src/app/SkyContextProjectionTypeCodec.cpp
    src/app/SkyContextProjectionTypeCodec.hpp
    src/render/SkyRenderStylePolicy.cpp
    src/render/SkyRenderStylePolicy.hpp
    src/settings/SkyCacheFiles.cpp
    src/settings/SkyCacheFiles.hpp
    src/settings/SkySettingsKeyBuilder.cpp
    src/settings/SkySettingsKeyBuilder.hpp
    src/settings/SkyTimelineViewDefaultsPolicy.hpp
    src/app/SkyContextTextFormatter.cpp
    src/app/SkyContextTextFormatter.hpp
    src/app/SkyContextTimeCodec.cpp
    src/app/SkyContextTimeCodec.hpp
    src/app/SkyContextUtcDateTimeTextCodec.cpp
    src/app/SkyContextUtcDateTimeTextCodec.hpp
    src/app/SkyLocationController.cpp
    src/app/SkyLocationController.hpp
    src/app/SkyLocationState.hpp
    src/app/SkySearchController.cpp
    src/app/SkySearchController.hpp
    src/app/SkySearchState.hpp
    src/app/SkySelectedCityChange.hpp
    src/app/SkyTimelineCatchUpResult.hpp
    src/app/SkyTimelineController.cpp
    src/app/SkyTimelineController.hpp
    src/app/SkyTimelineState.hpp
    src/app/SkyViewController.cpp
    src/app/SkyViewController.hpp
    src/app/SkyViewState.hpp
    src/render/SkyRenderBodyBuilder.cpp
    src/render/SkyRenderBodyBuilder.hpp
    src/render/SkyRenderBuilders.cpp
    src/render/SkyRenderBuilders.hpp
    src/render/SkyRenderFrame.hpp
    src/render/SkyRenderGlyph.hpp
    src/render/SkyRenderLabel.hpp
    src/render/SkyRenderLine.hpp
    src/render/SkyRenderPoint.hpp
    src/render/SkyRenderHorizontalLookup.cpp
    src/render/SkyRenderHorizontalLookup.hpp
    src/render/SkyRenderLabelBuilder.cpp
    src/render/SkyRenderLabelBuilder.hpp
    src/render/SkyRenderGlyphTessellator.cpp
    src/render/SkyRenderGlyphTessellator.hpp
    src/render/SkyReferenceOverlayLabelBuilder.cpp
    src/render/SkyReferenceOverlayLabelBuilder.hpp
    src/settings/SkySettingsSnapshotCodecs.cpp
    src/settings/SkySettingsSnapshotCodecs.hpp
    src/settings/SkyCatalogSourceSettingsSnapshot.hpp
    src/settings/SkyLocationSettingsSnapshot.hpp
    src/settings/SkyLoggingSettingsSnapshot.hpp
    src/settings/SkySearchSettingsSnapshot.hpp
    src/settings/SkyStateSettingsSnapshot.hpp
    src/settings/SkyTimelineSettingsSnapshot.hpp
    src/settings/SkyViewSettingsSnapshot.hpp
    src/settings/SkySettingsStore.cpp
    src/settings/SkySettingsStore.hpp
    src/settings/SkyEphemerisUserSettingsSnapshot.hpp
    src/settings/SkyPersistedStateSnapshot.hpp
    src/settings/SkySettingsValueCodecs.cpp
    src/settings/SkySettingsValueCodecs.hpp
    src/settings/SkyEphemerisDataCacheSettings.cpp
    src/settings/SkyEphemerisDataCacheSettings.hpp
    src/settings/SkyLegacyCatalogCacheStore.cpp
    src/settings/SkyLegacyCatalogCacheStore.hpp
    src/settings/SkyWindowSizeSettings.cpp
    src/settings/SkyWindowSizeSettings.hpp
    src/render/SkyViewportGeometry.cpp
    src/render/SkyViewportGeometry.hpp
    src/render/SkyViewportGeometryInput.hpp
    src/render/SkyViewportLineSegment.hpp
    src/render/SkyViewportSegmentSink.hpp
    src/render/SkyViewportItem.cpp
    src/render/SkyViewportItem.hpp
    src/render/SkyViewportSceneGraph.cpp
    src/render/SkyViewportSceneGraph.hpp
    src/render/SkyViewportSceneRootNode.cpp
    src/render/SkyViewportSceneRootNode.hpp
)

target_include_directories(skygate-ui-lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
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

qt_add_resources(skygateUiEphemerisSources resources/ephemeris.qrc)
target_sources(skygate-ui PRIVATE ${skygateUiEphemerisSources})

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
        qml/preferences/PreferencesEngineSection.qml
        qml/preferences/PreferencesAppearanceSection.qml
        qml/preferences/PreferencesCatalogSection.qml
        qml/preferences/PreferencesCityPicker.qml
        qml/preferences/PreferencesTimeZonePicker.qml
        qml/controls/PreferencesActionButton.qml
        qml/controls/PreferencesCheckBox.qml
        qml/controls/PreferencesComboBox.qml
        qml/controls/PreferencesCompactActionButton.qml
        qml/controls/PreferencesGroupTitle.qml
        qml/controls/PreferencesSearchablePickerShell.qml
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
        qml/status/StatusDegradationPopup.qml
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
