
set(CC_PREFIX aarch64-none-elf-)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR "arm64")
set(CMAKE_CROSSCOMPILING True)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_AR ${CC_PREFIX}ar CACHE STRING "AR" FORCE)
set(CMAKE_C_COMPILER  ${CC_PREFIX}gcc CACHE STRING "CC" FORCE)
set(CMAKE_ASM_COMPILER ${CC_PREFIX}gcc CACHE STRING "AS" FORCE)
set(CMAKE_CXX_COMPILER ${CC_PREFIX}g++ CACHE STRING "CXX" FORCE)

set(ToolchainPrefix ${CC_PREFIX} CACHE STRING "Toolchain prefix." FORCE)
set(ToolchainObjdump ${CC_PREFIX}objdump CACHE STRING "Objdump executable." FORCE)
set(ToolchainObjdumpFlags -SCdtx CACHE STRING "Objdump flags." FORCE)

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
set(OBJECT_GEN_FLAGS "-ffunction-sections -fdata-sections")

set(CMAKE_OBJCOPY ${CC_PREFIX}objcopy)
set(CMAKE_OBJDUMP ${CC_PREFIX}objdump)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
   set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
   set(CMAKE_ASM_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

set(LIBRARY_OUTPUT_PATH  ${CMAKE_BINARY_DIR}/lib)

set(DEFAULT_C_FLAGS_SECURITY -D_FORTIFY_SOURCE=2  CACHE STRING "Security build flags." FORCE)
set(DEFAULT_C_FLAGS_SECURITY_STRONG ${DEFAULT_C_FLAGS_SECURITY} -z noexecstack -fvisibility=hidden CACHE STRING "Strong Security build flags." FORCE)
set(DEFAULT_C_FLAGS_CLEAN_CODE -ffunction-sections -fdata-sections -fno-zero-initialized-in-bss -mstrict-align CACHE STRING "Code cleanliness build flags." FORCE)
set(DEFAULT_C_FLAGS_WARNINGS -Werror -Wall -Wextra -Wformat-security -Wformat -Wno-sign-compare -Wno-maybe-uninitialized -Wno-int-to-pointer-cast -Wno-stringop-truncation -Wno-array-bounds CACHE STRING "Warning build flags." FORCE)
if(CORE STREQUAL "A76")
    set(DEFAULT_C_FLAGS_PROCESSOR_TUNE -march=armv8-a+crc -mtune=cortex-a76 CACHE STRING "processor tuning build flags" FORCE)
    add_compile_definitions(configBOOT_CORE=2)
else()
    set(DEFAULT_C_FLAGS_PROCESSOR_TUNE -march=armv8-a+crc -mtune=cortex-a55 CACHE STRING "processor tuning build flags" FORCE)
    add_compile_definitions(configBOOT_CORE=0)
endif()

set(DEFAULT_C_FLAGS_DEBUG -O0 -g3 -DGUEST ${DEFAULT_C_FLAGS_SECURITY} ${DEFAULT_C_FLAGS_WARNINGS} ${DEFAULT_C_FLAGS_CLEAN_CODE} ${DEFAULT_C_FLAGS_PROCESSOR_TUNE} )

set(DEFAULT_C_FLAGS_RELEASE -O2 ${DEFAULT_C_FLAGS_SECURITY_STRONG} ${DEFAULT_C_FLAGS_WARNINGS} ${DEFAULT_C_FLAGS_CLEAN_CODE} ${DEFAULT_C_FLAGS_PROCESSOR_TUNE} )

set(FREERTOS_CONFIG_HEADER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/Demo/SOCFPGA)

if(NOT WIN32)
  string(ASCII 27 Esc)
  set(ColourReset "${Esc}[m")
  set(ColourBold  "${Esc}[1m")
  set(Red         "${Esc}[31m")
  set(Green       "${Esc}[32m")
  set(Yellow      "${Esc}[33m")
  set(Blue        "${Esc}[34m")
  set(White       "${Esc}[37m")
  set(BoldRed     "${Esc}[1;31m")
  set(BoldGreen   "${Esc}[1;32m")
  set(BoldYellow  "${Esc}[1;33m")
  set(BoldBlue    "${Esc}[1;34m")
  set(BoldWhite   "${Esc}[1;37m")
endif()

message(STATUS "${BoldWhite}Host System     :  ${CMAKE_SYSTEM_NAME}${ColourReset}")
message(STATUS "${BoldWhite}C COMPILER      :  ${CMAKE_C_COMPILER}${ColourReset}")

add_link_options(
    -Wl,--wrap=__retarget_lock_init
    -Wl,--wrap=__retarget_lock_init_recursive
    -Wl,--wrap=__retarget_lock_close
    -Wl,--wrap=__retarget_lock_close_recursive
    -Wl,--wrap=__retarget_lock_acquire
    -Wl,--wrap=__retarget_lock_acquire_recursive
    -Wl,--wrap=__retarget_lock_try_acquire
    -Wl,--wrap=__retarget_lock_try_acquire_recursive
    -Wl,--wrap=__retarget_lock_release
    -Wl,--wrap=__retarget_lock_release_recursive
    -Wl,-gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -static
    --specs=nosys.specs
    -z noexecstack
)
