include_guard(GLOBAL)

function(_skygate_find_program_or_xcrun toolName outputVariable)
    string(MAKE_C_IDENTIFIER "${outputVariable}" skygateToolSearchVariable)
    set(skygateToolSearchVariable "_${skygateToolSearchVariable}_SEARCH")
    find_program(${skygateToolSearchVariable} NAMES ${toolName})
    set(skygateToolExecutable "${${skygateToolSearchVariable}}")

    if(NOT skygateToolExecutable AND APPLE)
        find_program(skygateXcrunExecutable NAMES xcrun)
        if(skygateXcrunExecutable)
            execute_process(
                COMMAND "${skygateXcrunExecutable}" --find ${toolName}
                OUTPUT_VARIABLE skygateXcrunToolPath
                RESULT_VARIABLE skygateXcrunResult
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(skygateXcrunResult EQUAL 0 AND EXISTS "${skygateXcrunToolPath}")
                set(skygateToolExecutable "${skygateXcrunToolPath}")
            endif()
        endif()
    endif()

    set(${outputVariable} "${skygateToolExecutable}" PARENT_SCOPE)
endfunction()

function(skygate_enable_coverage)
    if(NOT SKYGATE_ENABLE_COVERAGE)
        return()
    endif()

    if(NOT SKYGATE_BUILD_TESTS)
        message(FATAL_ERROR "SKYGATE_ENABLE_COVERAGE requires SKYGATE_BUILD_TESTS=ON.")
    endif()

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Coverage is intended for Debug builds; current CMAKE_BUILD_TYPE is '${CMAKE_BUILD_TYPE}'.")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate -fcoverage-mapping)

        set(SKYGATE_COVERAGE_ENGINE "llvm" CACHE INTERNAL "Coverage report engine")
        _skygate_find_program_or_xcrun(llvm-cov SKYGATE_LLVM_COV_EXECUTABLE)
        _skygate_find_program_or_xcrun(llvm-profdata SKYGATE_LLVM_PROFDATA_EXECUTABLE)
        set(SKYGATE_LLVM_COV_EXECUTABLE "${SKYGATE_LLVM_COV_EXECUTABLE}" CACHE FILEPATH "llvm-cov executable")
        set(SKYGATE_LLVM_PROFDATA_EXECUTABLE "${SKYGATE_LLVM_PROFDATA_EXECUTABLE}" CACHE FILEPATH "llvm-profdata executable")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)

        set(SKYGATE_COVERAGE_ENGINE "gcov" CACHE INTERNAL "Coverage report engine")
        find_program(SKYGATE_GCOVR_EXECUTABLE NAMES gcovr)
    else()
        message(FATAL_ERROR
            "SKYGATE_ENABLE_COVERAGE supports Clang, AppleClang, and GNU compilers; "
            "current compiler is ${CMAKE_CXX_COMPILER_ID}."
        )
    endif()
endfunction()

function(_skygate_collect_coverage_test_targets directory outputVariable)
    get_property(skygateDirectoryTargets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(skygateSubdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)

    set(skygateCoverageTargets)
    foreach(skygateTarget IN LISTS skygateDirectoryTargets)
        if(NOT TARGET ${skygateTarget})
            continue()
        endif()

        get_target_property(skygateTargetType ${skygateTarget} TYPE)
        if(skygateTargetType STREQUAL "EXECUTABLE" AND skygateTarget MATCHES "-tests$")
            list(APPEND skygateCoverageTargets ${skygateTarget})
        endif()
    endforeach()

    foreach(skygateSubdirectory IN LISTS skygateSubdirectories)
        _skygate_collect_coverage_test_targets("${skygateSubdirectory}" skygateSubdirectoryTargets)
        list(APPEND skygateCoverageTargets ${skygateSubdirectoryTargets})
    endforeach()

    set(${outputVariable} ${skygateCoverageTargets} PARENT_SCOPE)
endfunction()

function(skygate_add_coverage_targets)
    if(NOT SKYGATE_ENABLE_COVERAGE)
        return()
    endif()

    _skygate_collect_coverage_test_targets("${CMAKE_SOURCE_DIR}" skygateCoverageTestTargets)
    list(REMOVE_DUPLICATES skygateCoverageTestTargets)

    if(NOT skygateCoverageTestTargets)
        message(WARNING "No SkyGate test executable targets were found for coverage reports.")
        return()
    endif()

    set(skygateCoverageDir "${CMAKE_BINARY_DIR}/coverage")

    if(SKYGATE_COVERAGE_ENGINE STREQUAL "llvm")
        if(NOT SKYGATE_LLVM_COV_EXECUTABLE OR NOT SKYGATE_LLVM_PROFDATA_EXECUTABLE)
            message(WARNING
                "llvm-cov and/or llvm-profdata were not found; coverage instrumentation is enabled, "
                "but report targets will not be generated."
            )
            return()
        endif()

        set(skygateCoverageObjectPaths)
        foreach(skygateTarget IN LISTS skygateCoverageTestTargets)
            list(APPEND skygateCoverageObjectPaths "$<TARGET_FILE:${skygateTarget}>")
        endforeach()

        add_custom_target(skygate-coverage-run
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${skygateCoverageDir}/profiles"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${skygateCoverageDir}/profiles"
            COMMAND
                ${CMAKE_COMMAND} -E env
                    "LLVM_PROFILE_FILE=${skygateCoverageDir}/profiles/%p-%m.profraw"
                    "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure
            DEPENDS ${skygateCoverageTestTargets}
            USES_TERMINAL
            COMMENT "Run SkyGate tests with LLVM coverage instrumentation"
        )

        add_custom_target(skygate-coverage-text
            COMMAND
                ${CMAKE_COMMAND}
                    "-DSKYGATE_LLVM_COV_EXECUTABLE=${SKYGATE_LLVM_COV_EXECUTABLE}"
                    "-DSKYGATE_LLVM_PROFDATA_EXECUTABLE=${SKYGATE_LLVM_PROFDATA_EXECUTABLE}"
                    "-DSKYGATE_COVERAGE_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
                    "-DSKYGATE_COVERAGE_BINARY_DIR=${CMAKE_BINARY_DIR}"
                    "-DSKYGATE_COVERAGE_OUTPUT_DIR=${skygateCoverageDir}"
                    "-DSKYGATE_COVERAGE_FORMAT=text"
                    "-DSKYGATE_COVERAGE_OBJECTS=$<JOIN:${skygateCoverageObjectPaths},,>"
                    -P "${CMAKE_SOURCE_DIR}/cmake/SkyGateCoverageReport.cmake"
            DEPENDS skygate-coverage-run
            USES_TERMINAL
            COMMENT "Generate SkyGate LLVM text coverage report"
        )

        add_custom_target(skygate-coverage-html
            COMMAND
                ${CMAKE_COMMAND}
                    "-DSKYGATE_LLVM_COV_EXECUTABLE=${SKYGATE_LLVM_COV_EXECUTABLE}"
                    "-DSKYGATE_LLVM_PROFDATA_EXECUTABLE=${SKYGATE_LLVM_PROFDATA_EXECUTABLE}"
                    "-DSKYGATE_COVERAGE_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
                    "-DSKYGATE_COVERAGE_BINARY_DIR=${CMAKE_BINARY_DIR}"
                    "-DSKYGATE_COVERAGE_OUTPUT_DIR=${skygateCoverageDir}"
                    "-DSKYGATE_COVERAGE_FORMAT=html"
                    "-DSKYGATE_COVERAGE_OBJECTS=$<JOIN:${skygateCoverageObjectPaths},,>"
                    -P "${CMAKE_SOURCE_DIR}/cmake/SkyGateCoverageReport.cmake"
            DEPENDS skygate-coverage-run
            USES_TERMINAL
            COMMENT "Generate SkyGate LLVM HTML coverage report"
        )

        add_custom_target(skygate-coverage-codecov
            COMMAND
                ${CMAKE_COMMAND}
                    "-DSKYGATE_LLVM_COV_EXECUTABLE=${SKYGATE_LLVM_COV_EXECUTABLE}"
                    "-DSKYGATE_LLVM_PROFDATA_EXECUTABLE=${SKYGATE_LLVM_PROFDATA_EXECUTABLE}"
                    "-DSKYGATE_COVERAGE_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
                    "-DSKYGATE_COVERAGE_BINARY_DIR=${CMAKE_BINARY_DIR}"
                    "-DSKYGATE_COVERAGE_OUTPUT_DIR=${skygateCoverageDir}"
                    "-DSKYGATE_COVERAGE_FORMAT=lcov"
                    "-DSKYGATE_COVERAGE_OBJECTS=$<JOIN:${skygateCoverageObjectPaths},,>"
                    -P "${CMAKE_SOURCE_DIR}/cmake/SkyGateCoverageReport.cmake"
            DEPENDS skygate-coverage-run
            USES_TERMINAL
            COMMENT "Generate SkyGate LLVM LCOV coverage report for Codecov"
        )
    elseif(SKYGATE_COVERAGE_ENGINE STREQUAL "gcov")
        add_custom_target(skygate-coverage-run
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${skygateCoverageDir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${skygateCoverageDir}"
            COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure
            DEPENDS ${skygateCoverageTestTargets}
            USES_TERMINAL
            COMMENT "Run SkyGate tests with GCC coverage instrumentation"
        )

        if(SKYGATE_GCOVR_EXECUTABLE)
            set(skygateGcovrCoverageFilters
                --filter "${CMAKE_SOURCE_DIR}/libs/.*"
                --filter "${CMAKE_SOURCE_DIR}/apps/.*"
                --exclude "${CMAKE_SOURCE_DIR}/.*/tests/.*"
                --exclude "${CMAKE_BINARY_DIR}/.*"
                --exclude ".*/moc_[^/]*\\.cpp$"
                --exclude ".*/qrc_[^/]*\\.cpp$"
                --exclude ".*/\\.rcc/.*"
            )

            add_custom_target(skygate-coverage-text
                COMMAND
                    "${SKYGATE_GCOVR_EXECUTABLE}"
                        --root "${CMAKE_SOURCE_DIR}"
                        --object-directory "${CMAKE_BINARY_DIR}"
                        ${skygateGcovrCoverageFilters}
                        --txt
                        --output "${skygateCoverageDir}/coverage.txt"
                DEPENDS skygate-coverage-run
                USES_TERMINAL
                COMMENT "Generate SkyGate gcovr text coverage report"
            )

            add_custom_target(skygate-coverage-html
                COMMAND ${CMAKE_COMMAND} -E make_directory "${skygateCoverageDir}/html"
                COMMAND
                    "${SKYGATE_GCOVR_EXECUTABLE}"
                        --root "${CMAKE_SOURCE_DIR}"
                        --object-directory "${CMAKE_BINARY_DIR}"
                        ${skygateGcovrCoverageFilters}
                        --html-details
                        --output "${skygateCoverageDir}/html/index.html"
                DEPENDS skygate-coverage-run
                USES_TERMINAL
                COMMENT "Generate SkyGate gcovr HTML coverage report"
            )

            add_custom_target(skygate-coverage-codecov
                COMMAND
                    "${SKYGATE_GCOVR_EXECUTABLE}"
                        --root "${CMAKE_SOURCE_DIR}"
                        --object-directory "${CMAKE_BINARY_DIR}"
                        ${skygateGcovrCoverageFilters}
                        --xml-pretty
                        --output "${skygateCoverageDir}/coverage.xml"
                DEPENDS skygate-coverage-run
                USES_TERMINAL
                COMMENT "Generate SkyGate gcovr XML coverage report for Codecov"
            )
        else()
            message(WARNING
                "gcovr was not found; coverage instrumentation and skygate-coverage-run are enabled, "
                "but report targets will not be generated."
            )
        endif()
    endif()
endfunction()
