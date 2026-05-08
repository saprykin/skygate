cmake_minimum_required(VERSION 3.24)

foreach(skygateRequiredVariable
    SKYGATE_LLVM_COV_EXECUTABLE
    SKYGATE_LLVM_PROFDATA_EXECUTABLE
    SKYGATE_COVERAGE_SOURCE_DIR
    SKYGATE_COVERAGE_BINARY_DIR
    SKYGATE_COVERAGE_OUTPUT_DIR
    SKYGATE_COVERAGE_FORMAT
    SKYGATE_COVERAGE_OBJECTS
)
    if(NOT DEFINED ${skygateRequiredVariable} OR "${${skygateRequiredVariable}}" STREQUAL "")
        message(FATAL_ERROR "${skygateRequiredVariable} is required.")
    endif()
endforeach()

set(skygateCoverageProfileDir "${SKYGATE_COVERAGE_OUTPUT_DIR}/profiles")
set(skygateCoverageProfdata "${SKYGATE_COVERAGE_OUTPUT_DIR}/skygate.profdata")

file(GLOB_RECURSE skygateRawProfiles "${skygateCoverageProfileDir}/*.profraw")
if(NOT skygateRawProfiles)
    message(FATAL_ERROR "No LLVM raw coverage profiles found in ${skygateCoverageProfileDir}.")
endif()

execute_process(
    COMMAND
        "${SKYGATE_LLVM_PROFDATA_EXECUTABLE}"
            merge
            -sparse
            ${skygateRawProfiles}
            -o "${skygateCoverageProfdata}"
    RESULT_VARIABLE skygateProfdataResult
    OUTPUT_VARIABLE skygateProfdataOutput
    ERROR_VARIABLE skygateProfdataError
)
if(NOT skygateProfdataResult EQUAL 0)
    message(FATAL_ERROR
        "llvm-profdata failed with exit code ${skygateProfdataResult}.\n"
        "${skygateProfdataOutput}\n${skygateProfdataError}"
    )
endif()

string(REPLACE "," ";" skygateCoverageObjects "${SKYGATE_COVERAGE_OBJECTS}")
foreach(skygateCoverageObject IN LISTS skygateCoverageObjects)
    if(EXISTS "${skygateCoverageObject}")
        list(APPEND skygateExistingCoverageObjects "${skygateCoverageObject}")
    else()
        message(WARNING "Coverage object does not exist and will be skipped: ${skygateCoverageObject}")
    endif()
endforeach()

if(NOT skygateExistingCoverageObjects)
    message(FATAL_ERROR "No coverage object files were found.")
endif()

list(POP_FRONT skygateExistingCoverageObjects skygatePrimaryCoverageObject)
set(skygateAdditionalCoverageObjects)
foreach(skygateCoverageObject IN LISTS skygateExistingCoverageObjects)
    list(APPEND skygateAdditionalCoverageObjects "-object=${skygateCoverageObject}")
endforeach()

set(skygateIgnoreFilenameRegex
    "(/build[^/]*/|/CMakeFiles/|/tests/|/moc_[^/]*\\.cpp$|/qrc_[^/]*\\.cpp$|/\\.rcc/|/usr/|/Applications/Xcode.app/|/Library/Developer/|/Qt/|/qtbase/)"
)

if(SKYGATE_COVERAGE_FORMAT STREQUAL "text")
    execute_process(
        COMMAND
            "${SKYGATE_LLVM_COV_EXECUTABLE}"
                report
                "${skygatePrimaryCoverageObject}"
                ${skygateAdditionalCoverageObjects}
                "-instr-profile=${skygateCoverageProfdata}"
                "-ignore-filename-regex=${skygateIgnoreFilenameRegex}"
                "${SKYGATE_COVERAGE_SOURCE_DIR}/libs"
                "${SKYGATE_COVERAGE_SOURCE_DIR}/apps"
        RESULT_VARIABLE skygateCoverageResult
        OUTPUT_VARIABLE skygateCoverageOutput
        ERROR_VARIABLE skygateCoverageError
    )
    if(NOT skygateCoverageResult EQUAL 0)
        message(FATAL_ERROR
            "llvm-cov report failed with exit code ${skygateCoverageResult}.\n"
            "${skygateCoverageOutput}\n${skygateCoverageError}"
        )
    endif()

    set(skygateTextReport "${SKYGATE_COVERAGE_OUTPUT_DIR}/coverage.txt")
    file(WRITE "${skygateTextReport}" "${skygateCoverageOutput}")
    message("${skygateCoverageOutput}")
    message(STATUS "Coverage text report written to ${skygateTextReport}")
elseif(SKYGATE_COVERAGE_FORMAT STREQUAL "html")
    set(skygateHtmlReportDir "${SKYGATE_COVERAGE_OUTPUT_DIR}/html")
    file(REMOVE_RECURSE "${skygateHtmlReportDir}")
    file(MAKE_DIRECTORY "${skygateHtmlReportDir}")

    execute_process(
        COMMAND
            "${SKYGATE_LLVM_COV_EXECUTABLE}"
                show
                "${skygatePrimaryCoverageObject}"
                ${skygateAdditionalCoverageObjects}
                "-instr-profile=${skygateCoverageProfdata}"
                "-ignore-filename-regex=${skygateIgnoreFilenameRegex}"
                -format=html
                "-output-dir=${skygateHtmlReportDir}"
                -show-line-counts-or-regions
                -show-instantiations=false
                "${SKYGATE_COVERAGE_SOURCE_DIR}/libs"
                "${SKYGATE_COVERAGE_SOURCE_DIR}/apps"
        RESULT_VARIABLE skygateCoverageResult
        OUTPUT_VARIABLE skygateCoverageOutput
        ERROR_VARIABLE skygateCoverageError
    )
    if(NOT skygateCoverageResult EQUAL 0)
        message(FATAL_ERROR
            "llvm-cov show failed with exit code ${skygateCoverageResult}.\n"
            "${skygateCoverageOutput}\n${skygateCoverageError}"
        )
    endif()

    message(STATUS "Coverage HTML report written to ${skygateHtmlReportDir}/index.html")
elseif(SKYGATE_COVERAGE_FORMAT STREQUAL "lcov")
    execute_process(
        COMMAND
            "${SKYGATE_LLVM_COV_EXECUTABLE}"
                export
                "${skygatePrimaryCoverageObject}"
                ${skygateAdditionalCoverageObjects}
                "-instr-profile=${skygateCoverageProfdata}"
                "-ignore-filename-regex=${skygateIgnoreFilenameRegex}"
                -format=lcov
                "${SKYGATE_COVERAGE_SOURCE_DIR}/libs"
                "${SKYGATE_COVERAGE_SOURCE_DIR}/apps"
        RESULT_VARIABLE skygateCoverageResult
        OUTPUT_VARIABLE skygateCoverageOutput
        ERROR_VARIABLE skygateCoverageError
    )
    if(NOT skygateCoverageResult EQUAL 0)
        message(FATAL_ERROR
            "llvm-cov export failed with exit code ${skygateCoverageResult}.\n"
            "${skygateCoverageOutput}\n${skygateCoverageError}"
        )
    endif()

    set(skygateLcovReport "${SKYGATE_COVERAGE_OUTPUT_DIR}/coverage.lcov")
    file(WRITE "${skygateLcovReport}" "${skygateCoverageOutput}")
    message(STATUS "Coverage LCOV report written to ${skygateLcovReport}")
else()
    message(FATAL_ERROR "Unsupported coverage format: ${SKYGATE_COVERAGE_FORMAT}")
endif()
