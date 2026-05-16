# ============================================
# Host Environment Validation Script
# ============================================

message(STATUS "Host OS : ${CMAKE_HOST_SYSTEM_NAME}")

# --------------------------------------------
# Only run checks on Windows
# --------------------------------------------
if (NOT WIN32)
    message(STATUS "Host Env Check : Pass")
    return()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/win_helper.cmake)

message(STATUS "MSYS2 : ${MSYS2_ROOT}")

# --------------------------------------------
# 2. Check required MSYS2 tools/packages
# --------------------------------------------
function(check_msys_tool tool pkg)

    execute_process(
        COMMAND "${MSYS2_BASH}" "-lc" "command -v ${tool}"
        RESULT_VARIABLE res
        OUTPUT_VARIABLE out
        ERROR_QUIET
    )

    if (NOT res EQUAL 0)
        message(FATAL_ERROR
            "Missing MSYS2 tool: ${tool}\n"
            "Install using:\n"
            "pacman -S ${pkg}"
        )
    else()
        string(STRIP "${out}" out)
        message(STATUS "${tool} : ${out}")
    endif()
endfunction()

# message("Checking MSYS2 tools...")

check_msys_tool(git "git")
check_msys_tool(python "python")
check_msys_tool(sed "sed")
check_msys_tool(awk "gawk")
check_msys_tool(make "make")
check_msys_tool(ls "coreutils")

# GCC (MinGW)
execute_process(
    COMMAND "${MSYS2_BASH}" "-lc" "export PATH=/mingw64/bin:$PATH && command -v x86_64-w64-mingw32-gcc"
    RESULT_VARIABLE gcc_res
    OUTPUT_VARIABLE gcc_out
)

if (NOT gcc_res EQUAL 0)
    message(FATAL_ERROR
        "Missing MinGW GCC!\n"
        "Install using:\n"
        "pacman -S mingw-w64-x86_64-gcc"
    )
else()
    string(STRIP "${gcc_out}" gcc_out)
    message(STATUS "Mingw GCC : ${gcc_out}")
endif()

# OpenSSL
check_msys_tool(openssl "mingw-w64-x86_64-openssl")

# --------------------------------------------
# 3. Ninja MUST be present
# --------------------------------------------
find_program(NINJA_PATH ninja)

if (NOT NINJA_PATH)
    message(FATAL_ERROR
        "Ninja not found on PATH!\n"
        "Install Ninja and ensure it's in PATH.\n"
        "Download: https://ninja-build.org/ or\n"
        "winget install Ninja-build.Ninja"
    )
else()
    message(STATUS "Ninja : ${NINJA_PATH}")
endif()

# --------------------------------------------
# 4. Enforce Ninja generator in CMake
# --------------------------------------------
if (DEFINED CMAKE_GENERATOR)
    if (NOT CMAKE_GENERATOR MATCHES "Ninja")
        message(FATAL_ERROR
            "CMake must be invoked with Ninja on Windows!\n"
            "Use:\n"
            "cmake -G Ninja ..."
        )
    endif()
else()
    message(WARNING
        "CMAKE_GENERATOR not defined (script mode).\n"
        "Ensure you invoke CMake with: -G Ninja"
    )
endif()

message(STATUS "Host Env Check : Pass")

#make symlink for external toolchain inside mingw
ext_toolchain_linkmingw()
