if(NOT DEFINED SKYGATE_RUNTIME_OUTPUT_DIR OR SKYGATE_RUNTIME_OUTPUT_DIR STREQUAL "")
    message(FATAL_ERROR "SKYGATE_RUNTIME_OUTPUT_DIR is required.")
endif()

foreach(skygateRuntimeDll IN LISTS SKYGATE_RUNTIME_DLLS)
    if(EXISTS "${skygateRuntimeDll}")
        file(COPY "${skygateRuntimeDll}" DESTINATION "${SKYGATE_RUNTIME_OUTPUT_DIR}")
    endif()
endforeach()
