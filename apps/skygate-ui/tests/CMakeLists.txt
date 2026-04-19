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
add_skygate_ui_qtest(skygate-ui-settings-store-tests SkySettingsStoreTests.cpp)
