cmake_minimum_required(VERSION 3.5...3.28)

#define these for qspi image generation

include(${CMAKE_CURRENT_SOURCE_DIR}/../common/download_dep.cmake)

include(FetchContent)

set(FREERTOS_FATFS y)
set(FREERTOS_USB y)
set(FREERTOS_TCPIP y)
set(FREERTOS_LIBRSU n)
set(FREERTOS_LIBFCS y)

FetchContent_Declare(
    freertos
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/../../
)

# Get the freertos repo
FetchContent_MakeAvailable(freertos)

# project
project(multi_thread_sample C CXX ASM)

set(LINKER_SCRIPT "${freertos_SOURCE_DIR}/lscript.ld")
include(${freertos_SOURCE_DIR}/tools/target_socfpga.cmake)

# target
add_executable(${PROJECT_NAME}.elf)

#Add image generation
generate_bin_file(${PROJECT_NAME}.elf)
add_sd_image(${PROJECT_NAME}.elf)
add_qspi_image(${PROJECT_NAME}.elf)


# link to the baremetal library
target_link_libraries(${PROJECT_NAME}.elf
    PRIVATE freertos_socfpga FCS
)

#Sample source files
set(SAMPLE_SRCS
	${CMAKE_SOURCE_DIR}/../dma/dma_sample.c
	${CMAKE_SOURCE_DIR}/../bridge/bridge_sample.c
	${CMAKE_SOURCE_DIR}/../bridge/hps2fpga_bridge.c
	${CMAKE_SOURCE_DIR}/../bridge/lwhps2fpga_bridge.c
	${CMAKE_SOURCE_DIR}/../fatfs/fatfs_sample.c
	${CMAKE_SOURCE_DIR}/../fcs/fcs_sample.c
	${CMAKE_SOURCE_DIR}/../i2c/i2c_sample.c
	${CMAKE_SOURCE_DIR}/../i3c/i3c_sample.c
	${CMAKE_SOURCE_DIR}/../qspi/qspi_sample.c
	${CMAKE_SOURCE_DIR}/../sdm_mailbox/sdm_mbox_sample.c
	${CMAKE_SOURCE_DIR}/../sdmmc/sdmmc_sample.c
	${CMAKE_SOURCE_DIR}/../seu/seu_sample.c
	${CMAKE_SOURCE_DIR}/../spi/spi_sample.c
	${CMAKE_SOURCE_DIR}/../timer/timer_sample.c
	${CMAKE_SOURCE_DIR}/../usb3/usb3_sample.c
	${freertos_SOURCE_DIR}/samples/common/socfpga_mmc.c
	)

target_include_directories(${PROJECT_NAME}.elf
	PRIVATE
	${freertos_SOURCE_DIR}/samples/common/
	)
# sources
target_sources(${PROJECT_NAME}.elf
    PRIVATE
	${CMAKE_SOURCE_DIR}/main.c
	${SAMPLE_SRCS}
	)

# specify linker script
target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${LINKER_SCRIPT}
)
