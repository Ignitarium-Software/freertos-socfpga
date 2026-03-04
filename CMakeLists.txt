cmake_minimum_required(VERSION 3.15.0)

set(FREERTOS_TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ARCH freertos_aarch64)
include(FetchContent)

if(NOT(SOC STREQUAL "AGILEX3" OR SOC STREQUAL "AGILEX5"))
    message(STATUS "Defaulting SOC to AGILEX5.")
    set(SOC "AGILEX5" CACHE STRING "Choose the SOC." FORCE)
endif()

if(SOC STREQUAL "AGILEX5")
    if(NOT(CORE STREQUAL "A55" OR CORE STREQUAL "A76"))
        message(STATUS "Defaulting boot core to CORTEX A55.")
        set(CORE "A55" CACHE STRING "Choose the boot core." FORCE)
    endif()
    add_compile_definitions(AGILEX5)
elseif(SOC STREQUAL "AGILEX3")
    set(CORE "A55" CACHE STRING "Only A55 core." FORCE)
    add_compile_definitions(AGILEX3)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/tools/target_socfpga.cmake)

find_program(TOOLCHAIN ${CMAKE_C_COMPILER} NO_CACHE)
if(NOT TOOLCHAIN)
    message("${Red}Compiler toolchain not found...${ColourReset}")
    message(FATAL_ERROR "${Red}Export toolchain path and run cmake again... ${colourReset}")
endif()

# Default to Release build type
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Defaulting build type to Release.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

#if CMAKE_BUILD_TYPE is Release, manually override the build flags.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "Release flags" FORCE)
    set(CMAKE_ASM_FLAGS_RELEASE "" CACHE STRING "Release flags" FORCE)
    set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "Release flags" FORCE)
endif()

message(STATUS "SOC : ${SOC}")
message(STATUS "Core : ${CORE}")
message(STATUS "Build Type : ${CMAKE_BUILD_TYPE}")

option(SMP "Enable FreeRTOS SMP" OFF)

if(SMP)
    message(STATUS "SMP : ON")
else()
    message(STATUS "SMP : OFF")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/tools/socfpga_build.cmake)

project(RTOSApp C CXX ASM)

add_custom_target(apply_smp_patch ALL
    COMMAND ${CMAKE_COMMAND}
    -DROOT_DIR="${FREERTOS_TOP_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/apply_smp_patch.cmake"
)

add_library(freertos_socfpga STATIC)

# Set compile options based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(freertos_socfpga PUBLIC ${DEFAULT_C_FLAGS_DEBUG})
else()
    target_compile_options(freertos_socfpga PRIVATE ${DEFAULT_C_FLAGS_RELEASE})
endif()

add_subdirectory(FreeRTOS)
add_dependencies(freertos_kernel apply_smp_patch)
add_subdirectory(drivers)

get_property(driver_include_dirs TARGET socfpga_drivers PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(freertos_socfpga PUBLIC ${driver_include_dirs})

if (FREERTOS_FATFS )
    #Fatfs depends on tinyusb stack. So we build tinyusb stack first followed by fatfs stack
    include(${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/hw/bsp/socfpga/family.cmake)

    set(FREERTOS_PLUS_FAT_FETCH_FREERTOS "OFF" CACHE STRING "" FORCE)
    set(FREERTOS_PLUS_FAT_TEST_CONFIGURATION "CUSTOM" CACHE STRING "" FORCE)
    set(FREERTOS_PLUS_FAT_PORT "A_CUSTOM_PORT" CACHE STRING "" FORCE)

    add_subdirectory(FreeRTOS/Fat-Fs/portable/SOCFPGA/)
    add_subdirectory(FreeRTOS/Fat-Fs)
    target_link_libraries(freertos_socfpga freertos_plus_fat freertos_plus_fat_port agilex5-tinyusb)
endif()

if(FREERTOS_LIBFCS)
   #Need the libFCS to be built as static lib
    set(NEED_STATIC_LIB ON)
    add_subdirectory(fcs)

    target_link_libraries(FCS PRIVATE freertos_plus_fat_port_common freertos_plus_fat)
    target_link_libraries(freertos_socfpga FCS)
endif()

if(FREERTOS_LIBRSU)
    #lib RSU library setup
    set(NEED_STATIC_LIB ON)
    add_subdirectory(rsu)

    target_link_libraries(uniLibRSU freertos_plus_fat_port_common freertos_plus_fat)
    target_link_libraries(freertos_socfpga uniLibRSU)
endif()

#link freertos_kernel with freertos_socfpga
target_link_libraries(freertos_socfpga freertos_kernel)

target_sources(freertos_socfpga PRIVATE
    ${MAIN_SOURCE_SRCS}
    $<TARGET_OBJECTS:socfpga_drivers>
    )
