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

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/support
            ${CMAKE_CURRENT_SOURCE_DIR}/app
            ${CMAKE_CURRENT_SOURCE_DIR}/catalog
            ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics
            ${CMAKE_CURRENT_SOURCE_DIR}/location
            ${CMAKE_CURRENT_SOURCE_DIR}/qml
            ${CMAKE_CURRENT_SOURCE_DIR}/render
            ${CMAKE_CURRENT_SOURCE_DIR}/scene
            ${CMAKE_CURRENT_SOURCE_DIR}/search
            ${CMAKE_CURRENT_SOURCE_DIR}/selection
            ${CMAKE_CURRENT_SOURCE_DIR}/settings
            ${CMAKE_CURRENT_SOURCE_DIR}/theme
            ${CMAKE_CURRENT_SOURCE_DIR}/time
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
    scene/SkySceneModelFrameTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-scene-model-selection-inspector-tests
    scene/SkySceneModelSelectionInspectorTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-scene-model-trail-tests
    scene/SkySceneModelTrailTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-scene-model-deep-sky-tests
    scene/SkySceneModelDeepSkyTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-scene-model-layer-visibility-tests
    scene/SkySceneModelLayerVisibilityTests.cpp
    TIMEOUT 60
)
add_skygate_ui_qtest(skygate-ui-location-catalog-model-tests location/LocationCatalogModelTests.cpp)
add_skygate_ui_qtest(skygate-ui-time-zone-catalog-model-tests time/TimeZoneCatalogModelTests.cpp)
add_skygate_ui_qtest(skygate-ui-log-capture-tests diagnostics/LogCaptureTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-context-controller-view-location-tests
    app/SkyContextControllerViewLocationTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-context-controller-appearance-settings-tests
    app/SkyContextControllerAppearanceSettingsTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-context-controller-timeline-tests
    app/SkyContextControllerTimelineTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-context-controller-search-tracking-tests
    app/SkyContextControllerSearchTrackingTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-context-controller-night-catalog-tests
    app/SkyContextControllerNightCatalogTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(skygate-ui-time-controller-tests time/SkyTimeControllerTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-logging-tests
    diagnostics/SkyLoggingTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ui_qtest(
    skygate-ui-settings-store-tests
    settings/SkySettingsStoreTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ui_qtest(
    skygate-ui-object-search-model-tests
    search/SkyObjectSearchModelTests.cpp
    TIMEOUT 45
)
add_skygate_ui_qtest(
    skygate-ui-theme-repository-tests
    theme/SkyThemeRepositoryTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ui_qtest(skygate-ui-overlay-layer-settings-tests theme/SkyOverlayLayerSettingsTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-catalog-cache-controller-tests
    catalog/SkyCatalogCacheControllerTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ui_qtest(skygate-ui-active-catalog-builder-tests catalog/SkyActiveCatalogBuilderTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-catalog-download-service-tests
    catalog/CatalogDownloadServiceTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-catalog-coordinator-download-tests
    catalog/CatalogCoordinatorDownloadTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(
    skygate-ui-sky-catalog-import-workflow-tests
    catalog/SkyCatalogImportWorkflowTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(skygate-ui-catalog-payload-parse-service-tests catalog/CatalogPayloadParseServiceTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-performance-guard-tests
    diagnostics/PerformanceGuardTests.cpp
    LABELS integration perf platform slow advisory
    TIMEOUT 120
)
add_skygate_ui_qtest(
    skygate-ui-sky-catalog-manager-tests
    catalog/SkyCatalogManagerTests.cpp
    LABELS integration
    TIMEOUT 60
)
add_skygate_ui_qtest(skygate-ui-sky-catalog-runtime-tests catalog/SkyCatalogRuntimeTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-hit-target-index-tests selection/SkyHitTargetIndexTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-object-trail-builder-tests selection/SkyObjectTrailBuilderTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-render-builders-tests render/SkyRenderBuildersTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-catalog-internals-tests catalog/SkyCatalogInternalsTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-scene-composition-tests scene/SkySceneCompositionTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-scene-frame-pipeline-tests scene/SkySceneFramePipelineTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-scene-overlay-adapter-tests scene/SkySceneOverlayAdapterTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-selection-overlay-builder-tests selection/SkySelectionOverlayBuilderTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-settings-codecs-tests settings/SkySettingsCodecsTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-viewport-internals-tests render/SkyViewportInternalsTests.cpp)
add_skygate_ui_qtest(
    skygate-ui-qml-component-load-tests
    qml/QmlComponentLoadTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-date-time-popup-tests
    qml/QmlDateTimePopupTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-interaction-layer-tests
    qml/QmlInteractionLayerTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-main-window-tests
    qml/QmlMainWindowTests.cpp
    LABELS qml integration
    TIMEOUT 90
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-night-conditions-popup-tests
    qml/QmlNightConditionsPopupTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-catalog-tests
    qml/QmlPreferencesCatalogTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-draft-tests
    qml/QmlPreferencesDraftTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-appearance-tests
    qml/QmlPreferencesAppearanceTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-location-time-tests
    qml/QmlPreferencesLocationTimeTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preferences-window-tests
    qml/QmlPreferencesWindowTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-preference-control-tests
    qml/QmlPreferenceControlTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-sky-overlay-layer-payload-rendering-tests
    qml/QmlSkyOverlayLayerPayloadRenderingTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-sky-overlay-layer-inspector-interaction-tests
    qml/QmlSkyOverlayLayerInspectorInteractionTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-viewport-rendering-tests
    qml/QmlViewportRenderingTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-main-window-rendering-tests
    qml/QmlMainWindowRenderingTests.cpp
    LABELS qml integration slow
    TIMEOUT 120
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-smoke-tests
    qml/QmlSmokeTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-status-footer-tests
    qml/QmlStatusFooterTests.cpp
    LABELS qml integration
    TIMEOUT 60
    ENVIRONMENT ${skygateQmlTestEnvironment}
)
add_skygate_ui_qtest(
    skygate-ui-qml-toolbar-tests
    qml/QmlToolbarTests.cpp
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
            SKYGATE_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../qml"
    )
endforeach()
