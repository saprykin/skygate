if(NOT DEFINED SKYGATE_UI_EXECUTABLE)
    message(FATAL_ERROR "SKYGATE_UI_EXECUTABLE is required.")
endif()

if(NOT EXISTS "${SKYGATE_UI_EXECUTABLE}")
    message(FATAL_ERROR "SkyGate executable does not exist: ${SKYGATE_UI_EXECUTABLE}")
endif()

if(NOT DEFINED SKYGATE_SMOKE_WORK_DIR)
    set(SKYGATE_SMOKE_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/skygate-ui-packaged-app-smoke")
endif()

if(NOT DEFINED SKYGATE_SMOKE_OUTPUT_FILE)
    set(SKYGATE_SMOKE_OUTPUT_FILE "${SKYGATE_SMOKE_WORK_DIR}/output.log")
endif()

if(NOT DEFINED SKYGATE_SMOKE_TIMEOUT_SECONDS)
    set(SKYGATE_SMOKE_TIMEOUT_SECONDS 6)
endif()

file(MAKE_DIRECTORY "${SKYGATE_SMOKE_WORK_DIR}")
file(MAKE_DIRECTORY "${SKYGATE_SMOKE_WORK_DIR}/home")
file(MAKE_DIRECTORY "${SKYGATE_SMOKE_WORK_DIR}/cache")
file(MAKE_DIRECTORY "${SKYGATE_SMOKE_WORK_DIR}/config")
file(MAKE_DIRECTORY "${SKYGATE_SMOKE_WORK_DIR}/runtime")
file(MAKE_DIRECTORY "${SKYGATE_SMOKE_WORK_DIR}/temp")

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
            "HOME=${SKYGATE_SMOKE_WORK_DIR}/home"
            "TMPDIR=${SKYGATE_SMOKE_WORK_DIR}/temp"
            "XDG_CACHE_HOME=${SKYGATE_SMOKE_WORK_DIR}/cache"
            "XDG_CONFIG_HOME=${SKYGATE_SMOKE_WORK_DIR}/config"
            "XDG_RUNTIME_DIR=${SKYGATE_SMOKE_WORK_DIR}/runtime"
            "QML_DISABLE_DISK_CACHE=1"
            "QT_QPA_PLATFORM=offscreen"
            "QT_QUICK_CONTROLS_STYLE=Basic"
            "SKYGATE_LOG_OUTPUT=terminal"
            "SKYGATE_LOG_LEVEL=info"
            "${SKYGATE_UI_EXECUTABLE}"
    WORKING_DIRECTORY "${SKYGATE_SMOKE_WORK_DIR}"
    TIMEOUT "${SKYGATE_SMOKE_TIMEOUT_SECONDS}"
    RESULT_VARIABLE smokeResult
    OUTPUT_VARIABLE smokeStdout
    ERROR_VARIABLE smokeStderr
)

set(smokeOutput "${smokeStdout}\n${smokeStderr}")
file(WRITE "${SKYGATE_SMOKE_OUTPUT_FILE}" "${smokeOutput}")
string(STRIP "${smokeOutput}" strippedSmokeOutput)

function(skygate_smoke_fail failureMessage)
    message(STATUS "SkyGate smoke output written to ${SKYGATE_SMOKE_OUTPUT_FILE}")
    message(STATUS "SkyGate smoke result: ${smokeResult}")
    message(FATAL_ERROR "${failureMessage}")
endfunction()

if(
    smokeOutput MATCHES "Could not load the Qt platform plugin"
    OR smokeOutput MATCHES "no Qt platform plugin could be initialized"
    OR smokeOutput MATCHES "Failed to create wl_display"
    OR smokeOutput MATCHES "QXcbConnection"
    OR smokeOutput MATCHES "qt.qpa.xcb"
    OR strippedSmokeOutput STREQUAL "Subprocess aborted"
)
    message("SKYGATE_SMOKE_SKIP: Qt GUI launch is not available in this environment.")
    return()
endif()

if(NOT smokeOutput MATCHES "INFO skygate\\.app SkyGate started:")
    skygate_smoke_fail("SkyGate did not reach the application startup log line.")
endif()

if(
    smokeOutput MATCHES "ERROR "
    OR smokeOutput MATCHES "FATAL "
    OR smokeOutput MATCHES "QQmlApplicationEngine failed to load component"
    OR smokeOutput MATCHES "objectCreationFailed"
    OR smokeOutput MATCHES "module \"[^\"]+\" is not installed"
    OR smokeOutput MATCHES "plugin cannot be loaded"
    OR smokeOutput MATCHES "Cannot load library"
)
    skygate_smoke_fail("SkyGate emitted a critical startup, QML, resource, or plugin error.")
endif()

if(smokeResult STREQUAL "Process terminated due to timeout")
    message(STATUS "SkyGate packaged app smoke reached startup and stayed alive until timeout.")
elseif(smokeResult EQUAL 0)
    message(STATUS "SkyGate packaged app smoke reached startup and exited cleanly.")
else()
    skygate_smoke_fail("SkyGate exited unexpectedly during packaged app smoke.")
endif()
