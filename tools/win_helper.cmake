#cmake helper file for windows specific ops

# --------------------------------------------
# 1. Locate MSYS2 installation
# --------------------------------------------
set(MSYS2_HINTS
    "C:/msys64"
    "C:/tools/msys64"
    "$ENV{USERPROFILE}/msys64"
)

set(MSYS2_ROOT "")

foreach(path IN LISTS MSYS2_HINTS)
    if (EXISTS "${path}/usr/bin/bash.exe")
        set(MSYS2_ROOT "${path}")
        break()
    endif()
endforeach()

if (MSYS2_ROOT STREQUAL "")
    message(FATAL_ERROR
        "MSYS2 not found!\n"
        "Expected at: C:/msys64 (or similar)\n"
        "Install from: https://www.msys2.org/ or\n"
        "winget install MSYS2.MSYS2"
    )
endif()

set(MSYS2_BASH "${MSYS2_ROOT}/usr/bin/bash.exe")

function(ext_toolchain_linkmingw)
    find_program(CROSS_GCC_PATH aarch64-none-elf-gcc)
    get_filename_component(CROSS_GCC_PATH_FOLDER ${CROSS_GCC_PATH} DIRECTORY)
    set(CROSS_GCC_PATH_FOLDER "${CROSS_GCC_PATH_FOLDER}/..")
    set(CROSS_MSYS_LINK "${MSYS2_ROOT}/opt/aarch-cc")

    string(REPLACE " " "\\ " ESCAPED "${CROSS_GCC_PATH_FOLDER}")
    string(REPLACE "(" "\\(" ESCAPED "${ESCAPED}")
    string(REPLACE ")" "\\)" ESCAPED "${ESCAPED}")

    if(NOT EXISTS "${CROSS_MSYS_LINK}")
        execute_process(
            COMMAND "${MSYS2_BASH}" "-lc" "ln -s ${ESCAPED} /opt/aarch-cc"
            RESULT_VARIABLE gcc_res
            OUTPUT_VARIABLE gcc_out
        )
    endif()
endfunction()

function(format_mingw_command command format_out)
    set(${format_out}
        "${MSYS2_ROOT}/usr/bin/env.exe"
        MSYSTEM=MINGW64
        ${MSYS2_BASH} -lc
        "${command}"
        PARENT_SCOPE
    )
endfunction()

function(execute_process_mingw command output_var result_var)
    message(STATUS "${MSYS2_ROOT}  --  ${MSYS2_BASH}")
    execute_process(
        COMMAND "${MSYS2_ROOT}/usr/bin/env.exe"
                MSYSTEM=MINGW64
                ${MSYS2_BASH} -lc "${command}"
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        RESULT_VARIABLE _res
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    set(${output_var} "${_out}" PARENT_SCOPE)
    set(${result_var} "${_res}" PARENT_SCOPE)
    set(${output_var}_ERR "${_err}" PARENT_SCOPE)
endfunction()
