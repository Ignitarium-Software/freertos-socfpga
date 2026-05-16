cmake_minimum_required(VERSION 3.5...3.28)

#set PFG variable to the pfg filename

include(${CMAKE_CURRENT_SOURCE_DIR}/../download_dep.cmake)

include(FetchContent)

set(FREERTOS_FATFS n)
set(FREERTOS_USB n)
set(FREERTOS_TCPIP n)
set(FREERTOS_LIBRSU n)
set(FREERTOS_LIBFCS n)

get_filename_component(FREERTOS_PATH
    "${CMAKE_SOURCE_DIR}/../../../../.."
    ABSOLUTE
)

FetchContent_Declare(
    freertos
    SOURCE_DIR ${FREERTOS_PATH}
)

# Get the freertos repo
FetchContent_MakeAvailable(freertos)

# project
project(freertos_hello_world C CXX ASM)

set(LINKER_SCRIPT "${freertos_SOURCE_DIR}/lscript.ld")
include(${freertos_SOURCE_DIR}/tools/target_socfpga.cmake)

# target
add_executable(${PROJECT_NAME}.elf)

#Add image generation
generate_bin_file(${PROJECT_NAME}.elf)

# link to the baremetal library
target_link_libraries(${PROJECT_NAME}.elf
    PRIVATE freertos_socfpga
)

# sources
target_sources(${PROJECT_NAME}.elf
    PRIVATE hello_world.c
)

# specify linker script
target_link_options(${PROJECT_NAME}.elf PRIVATE
   -T ${LINKER_SCRIPT}
)

add_sd_image(${PROJECT_NAME}.elf)
add_qspi_image(${PROJECT_NAME}.elf)
