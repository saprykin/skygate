find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

function(add_skygate_ui_qtest target_name source_file)
    cmake_parse_arguments(
        TEST
        ""
        "TIMEOUT"
        "ENVIRONMENT;LABELS"
        ${ARGN}
    )

    add_executable(${target_name}
        ${source_file}
    )

    set_target_properties(${target_name} PROPERTIES AUTOMOC ON)

    target_link_libraries(${target_name}
        PRIVATE
            Qt6::Test
            skygate::uiapp
            skygate::test-support
    )

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    add_test(
        NAME ${target_name}
        COMMAND $<TARGET_FILE:${target_name}>
    )

    if(NOT TEST_LABELS)
        set(TEST_LABELS unit)
    endif()
    if(NOT TEST_TIMEOUT)
        set(TEST_TIMEOUT 30)
    endif()

    list(REMOVE_DUPLICATES TEST_LABELS)
    set_tests_properties(${target_name}
        PROPERTIES
            LABELS "${TEST_LABELS}"
            TIMEOUT "${TEST_TIMEOUT}"
    )
    if(TEST_ENVIRONMENT)
        set_property(TEST ${target_name}
            PROPERTY ENVIRONMENT "${TEST_ENVIRONMENT}"
        )
    endif()
endfunction()

set(skygateUiPackagedAppSmokeWorkDir
    "${CMAKE_CURRENT_BINARY_DIR}/skygate-ui-packaged-app-smoke"
)

add_test(
    NAME skygate-ui-packaged-app-smoke
    COMMAND
        ${CMAKE_COMMAND}
            -DSKYGATE_UI_EXECUTABLE=$<TARGET_FILE:skygate-ui>
            -DSKYGATE_SMOKE_WORK_DIR=${skygateUiPackagedAppSmokeWorkDir}
            -DSKYGATE_SMOKE_OUTPUT_FILE=${skygateUiPackagedAppSmokeWorkDir}/output.log
            -P ${CMAKE_CURRENT_SOURCE_DIR}/PackagedAppSmoke.cmake
)

set_tests_properties(skygate-ui-packaged-app-smoke PROPERTIES
    LABELS "integration;platform"
    SKIP_REGULAR_EXPRESSION "SKYGATE_SMOKE_SKIP:"
    TIMEOUT 15
)

set(skygateQmlTestEnvironment
    QML_DISABLE_DISK_CACHE=1
    QT_QUICK_CONTROLS_STYLE=Basic
)

add_skygate_ui_qtest(
    skygate-ui-scene-model-frame-tests
    SkySceneModelFrameTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-scene-model-selection-inspector-tests
    SkySceneModelSelectionInspectorTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-scene-model-trail-tests
    SkySceneModelTrailTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-scene-model-deep-sky-tests
    SkySceneModelDeepSkyTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-scene-model-layer-visibility-tests
    SkySceneModelLayerVisibilityTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(skygate-ui-location-catalog-model-tests LocationCatalogModelTests.cpp)
add_skygate_ui_qtest(skygate-ui-time-zone-catalog-model-tests TimeZoneCatalogModelTests.cpp)
add_skygate_ui_qtest(skygate-ui-log-capture-tests LogCaptureTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-context-controller-view-location-tests
    SkyContextControllerViewLocationTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-context-controller-appearance-settings-tests
    SkyContextControllerAppearanceSettingsTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-context-controller-timeline-tests
    SkyContextControllerTimelineTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-context-controller-search-tracking-tests
    SkyContextControllerSearchTrackingTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-context-controller-night-catalog-tests
    SkyContextControllerNightCatalogTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(skygate-ui-time-controller-tests SkyTimeControllerTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-logging-tests
    SkyLoggingTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ui_qtest(
    skygate-ui-settings-store-tests
    SkySettingsStoreTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ui_qtest(
    skygate-ui-object-search-model-tests
    SkyObjectSearchModelTests.cpp
    TIMEOUT 45
)
add_skygate_ui_qtest(
    skygate-ui-theme-repository-tests
    SkyThemeRepositoryTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ui_qtest(skygate-ui-overlay-layer-settings-tests SkyOverlayLayerSettingsTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-catalog-cache-controller-tests
    SkyCatalogCacheControllerTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ui_qtest(skygate-ui-active-catalog-builder-tests SkyActiveCatalogBuilderTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-catalog-download-service-tests
    CatalogDownloadServiceTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-catalog-coordinator-download-tests
    CatalogCoordinatorDownloadTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-sky-catalog-import-workflow-tests
    SkyCatalogImportWorkflowTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(skygate-ui-catalog-payload-parse-service-tests CatalogPayloadParseServiceTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-performance-guard-tests
    PerformanceGuardTests.cpp
    LABELS integration perf platform slow advisory
    TIMEOUT 120
)
add_skygate_ui_qtest(
    skygate-ui-sky-catalog-manager-tests
    SkyCatalogManagerTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(skygate-ui-sky-catalog-runtime-tests SkyCatalogRuntimeTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-hit-target-index-tests SkyHitTargetIndexTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-object-trail-builder-tests SkyObjectTrailBuilderTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-render-builders-tests SkyRenderBuildersTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-catalog-internals-tests SkyCatalogInternalsTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-scene-composition-tests SkySceneCompositionTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-scene-frame-pipeline-tests SkySceneFramePipelineTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-scene-overlay-adapter-tests SkySceneOverlayAdapterTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-selection-overlay-builder-tests SkySelectionOverlayBuilderTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-settings-codecs-tests SkySettingsCodecsTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-viewport-internals-tests SkyViewportInternalsTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-qml-component-load-tests
    QmlComponentLoadTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-date-time-popup-tests
    QmlDateTimePopupTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-interaction-layer-tests
    QmlInteractionLayerTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-main-window-tests
    QmlMainWindowTests.cpp
    LABELS qml integration
    TIMEOUT 90
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-night-conditions-popup-tests
    QmlNightConditionsPopupTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-catalog-tests
    QmlPreferencesCatalogTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-draft-tests
    QmlPreferencesDraftTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-appearance-tests
    QmlPreferencesAppearanceTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-location-time-tests
    QmlPreferencesLocationTimeTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-window-tests
    QmlPreferencesWindowTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preference-control-tests
    QmlPreferenceControlTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-sky-overlay-layer-payload-rendering-tests
    QmlSkyOverlayLayerPayloadRenderingTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-sky-overlay-layer-inspector-interaction-tests
    QmlSkyOverlayLayerInspectorInteractionTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-viewport-rendering-tests
    QmlViewportRenderingTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-main-window-rendering-tests
    QmlMainWindowRenderingTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-smoke-tests
    QmlSmokeTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-status-footer-tests
    QmlStatusFooterTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-toolbar-tests
    QmlToolbarTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)

foreach(qml_test_target IN ITEMS
    skygate-ui-qml-component-load-tests
    skygate-ui-qml-date-time-popup-tests
    skygate-ui-qml-interaction-layer-tests
    skygate-ui-qml-main-window-tests
    skygate-ui-qml-night-conditions-popup-tests
    skygate-ui-qml-preferences-catalog-tests
    skygate-ui-qml-preferences-draft-tests
    skygate-ui-qml-preferences-appearance-tests
    skygate-ui-qml-preferences-location-time-tests
    skygate-ui-qml-preferences-window-tests
    skygate-ui-qml-preference-control-tests
    skygate-ui-qml-sky-overlay-layer-payload-rendering-tests
    skygate-ui-qml-sky-overlay-layer-inspector-interaction-tests
    skygate-ui-qml-viewport-rendering-tests
    skygate-ui-qml-main-window-rendering-tests
    skygate-ui-qml-smoke-tests
    skygate-ui-qml-status-footer-tests
    skygate-ui-qml-toolbar-tests
)
    target_compile_definitions(${qml_test_target}
        PRIVATE
            SKYGATE_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
endforeach()
