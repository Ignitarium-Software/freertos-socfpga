cmake_minimum_required(VERSION 3.5...3.28)

#define these for qspi image generation
include(${CMAKE_CURRENT_SOURCE_DIR}/../common/download_dep.cmake)
set(PFG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/initial_image.pfg" CACHE STRING "PFG path")

include(FetchContent)

set(FREERTOS_FATFS y)
set(FREERTOS_USB y)
set(FREERTOS_TCPIP n)
set(FREERTOS_LIBRSU y)
set(FREERTOS_LIBFCS n)

FetchContent_Declare(
    freertos
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/../../
)

# Get the freertos repo
FetchContent_MakeAvailable(freertos)
FetchContent_GetProperties(freertos)

# project
project(rsu_sample C CXX ASM)

set(LINKER_SCRIPT "${freertos_SOURCE_DIR}/lscript.ld")
include(${freertos_SOURCE_DIR}/tools/target_socfpga.cmake)

function(generate_bin_file target_name)
    if(target_name MATCHES "\\.elf$")
        string(REGEX REPLACE "\\.elf$" ".bin" BIN_FILE "${target_name}")
    else()
        set(BIN_FILE "${target_name}.bin")
    endif()
    string(REGEX REPLACE ".bin" ".asm" ASM_FILE ${BIN_FILE})
    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary ${target_name} ${BIN_FILE}
        COMMAND ${CMAKE_OBJDUMP} -D ${target_name} > ${ASM_FILE}
        COMMENT "Generating ${BIN_FILE} and ${ASM_FILE} from ${target_name}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
    )
    add_custom_target(${BIN_FILE} DEPENDS ${target_name})
endfunction()

# target
add_executable(${PROJECT_NAME}.elf
)

#Add image generation
generate_bin_file(${PROJECT_NAME}.elf)
add_sd_image(${PROJECT_NAME}.elf)
add_qspi_image(${PROJECT_NAME}.elf)


# link to the baremetal library
target_link_libraries(${PROJECT_NAME}.elf
    PRIVATE freertos_socfpga
)

file(GLOB SAMPLE_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# sources
target_sources(${PROJECT_NAME}.elf
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../main.c ${SAMPLE_SRCS}
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bin/app2.rpd
     DESTINATION ${CMAKE_BINARY_DIR}/qspi_atf_binaries/)
file(RENAME ${CMAKE_BINARY_DIR}/qspi_atf_binaries/app2.rpd ${CMAKE_BINARY_DIR}/qspi_atf_binaries/app2.bin)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bin/fip2.bin
     DESTINATION ${CMAKE_BINARY_DIR}/qspi_atf_binaries/)

# specify linker script
target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${LINKER_SCRIPT}
)

add_custom_command(
    TARGET qspi-image
    POST_BUILD
    COMMAND ${QUARTUS_PFG_EXECUTABLE} -c ${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE} app2.rpd -o hps_path=./bl2.hex -o mode=ASX4 -o bitswap=ON
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/qspi_atf_binaries
    COMMENT "Generating app2.rpd for QSPI image"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/qspi_atf_binaries/app2.rpd
    DEPENDS
        get_sof_file
        atf-bl2-bl31-qspi
        ${QUARTUS_PFG_EXECUTABLE}
    )
