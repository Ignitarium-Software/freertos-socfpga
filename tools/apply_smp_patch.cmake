set(PATCH_FILE "${CMAKE_CURRENT_LIST_DIR}/smp.patch")
set(PATCH_WORKDIR "${ROOT_DIR}/FreeRTOS/Source")
message("${PATCH_WORKDIR}")

if(NOT EXISTS "${PATCH_FILE}")
    message(FATAL_ERROR "smp.patch not found at ${PATCH_FILE}")
endif()

execute_process(
    COMMAND git apply --check "${PATCH_FILE}"
    WORKING_DIRECTORY "${PATCH_WORKDIR}"
    RESULT_VARIABLE CHECK_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

if(CHECK_RESULT EQUAL 0)
    message("Applying smp.patch")
    execute_process(
        COMMAND git apply "${PATCH_FILE}"
        WORKING_DIRECTORY "${PATCH_WORKDIR}"
        RESULT_VARIABLE APPLY_RESULT
)
endif()
