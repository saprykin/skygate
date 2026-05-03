find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

function(add_skygate_ui_qtest target_name source_file)
    add_executable(${target_name}
        ${source_file}
    )

    set_target_properties(${target_name} PROPERTIES AUTOMOC ON)

    target_link_libraries(${target_name}
        PRIVATE
            Qt6::Test
            skygate::uiapp
    )

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    add_test(
        NAME ${target_name}
        COMMAND $<TARGET_FILE:${target_name}>
    )
endfunction()

add_skygate_ui_qtest(skygate-ui-scene-model-tests SkySceneModelTests.cpp)
add_skygate_ui_qtest(skygate-ui-location-catalog-model-tests LocationCatalogModelTests.cpp)
add_skygate_ui_qtest(skygate-ui-context-controller-tests SkyContextControllerTests.cpp)
add_skygate_ui_qtest(skygate-ui-settings-store-tests SkySettingsStoreTests.cpp)
add_skygate_ui_qtest(skygate-ui-object-search-model-tests SkyObjectSearchModelTests.cpp)
add_skygate_ui_qtest(skygate-ui-theme-repository-tests SkyThemeRepositoryTests.cpp)
add_skygate_ui_qtest(skygate-ui-overlay-layer-settings-tests SkyOverlayLayerSettingsTests.cpp)
add_skygate_ui_qtest(skygate-ui-catalog-cache-controller-tests SkyCatalogCacheControllerTests.cpp)
add_skygate_ui_qtest(skygate-ui-active-catalog-builder-tests SkyActiveCatalogBuilderTests.cpp)
add_skygate_ui_qtest(skygate-ui-catalog-download-workflow-tests CatalogDownloadWorkflowTests.cpp)
add_skygate_ui_qtest(skygate-ui-catalog-payload-parse-service-tests CatalogPayloadParseServiceTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-catalog-manager-tests SkyCatalogManagerTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-hit-target-index-tests SkyHitTargetIndexTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-object-trail-builder-tests SkyObjectTrailBuilderTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-render-builders-tests SkyRenderBuildersTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-scene-frame-pipeline-tests SkySceneFramePipelineTests.cpp)
add_skygate_ui_qtest(skygate-ui-sky-selection-overlay-builder-tests SkySelectionOverlayBuilderTests.cpp)
add_skygate_ui_qtest(skygate-ui-qml-component-load-tests QmlComponentLoadTests.cpp)
add_skygate_ui_qtest(skygate-ui-qml-date-time-popup-tests QmlDateTimePopupTests.cpp)
add_skygate_ui_qtest(skygate-ui-qml-interaction-layer-tests QmlInteractionLayerTests.cpp)
add_skygate_ui_qtest(skygate-ui-qml-main-window-tests QmlMainWindowTests.cpp)
add_skygate_ui_qtest(skygate-ui-qml-preferences-tests QmlPreferencesTests.cpp)
add_skygate_ui_qtest(skygate-ui-qml-rendering-tests QmlRenderingTests.cpp)
add_skygate_ui_qtest(skygate-ui-qml-smoke-tests QmlSmokeTests.cpp)
add_skygate_ui_qtest(skygate-ui-qml-status-footer-tests QmlStatusFooterTests.cpp)
add_skygate_ui_qtest(skygate-ui-qml-toolbar-tests QmlToolbarTests.cpp)

foreach(qml_test_target IN ITEMS
    skygate-ui-qml-component-load-tests
    skygate-ui-qml-date-time-popup-tests
    skygate-ui-qml-interaction-layer-tests
    skygate-ui-qml-main-window-tests
    skygate-ui-qml-preferences-tests
    skygate-ui-qml-rendering-tests
    skygate-ui-qml-smoke-tests
    skygate-ui-qml-status-footer-tests
    skygate-ui-qml-toolbar-tests
)
    target_compile_definitions(${qml_test_target}
        PRIVATE
            SKYGATE_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
endforeach()
