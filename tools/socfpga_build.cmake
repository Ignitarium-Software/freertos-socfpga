
if(WIN32)
    include(${CMAKE_CURRENT_LIST_DIR}/win_helper.cmake)
endif()

if(NOT ATF_SOURCE)
    set(ATF_SOURCE ${FREERTOS_TOP_DIR}/tools/ATF CACHE STRING "Path to ATF git repository")
endif()

set(ATF_BUILD_COPY_DIR "${CMAKE_BINARY_DIR}/atf_copy" CACHE STRING "Path to ATF build copy directory")
set(TOOL_DIR "${FREERTOS_TOP_DIR}/tools" CACHE STRING "Path to tools directory")

set(SD_ARTIFACTS_DIR "${CMAKE_BINARY_DIR}/sd_atf_binaries" CACHE STRING "Path to SD artifacts directory")
set(EMMC_ARTIFACTS_DIR "${CMAKE_BINARY_DIR}/emmc_atf_binaries" CACHE STRING "Path to eMMC artifacts directory")
set(QSPI_ARTIFACTS_DIR "${CMAKE_BINARY_DIR}/qspi_atf_binaries" CACHE STRING "Path to QSPI artifacts directory")

#Create a cmake file to edit the ATF MACRO
set(_mmc_patch_script "${CMAKE_BINARY_DIR}/patch_mmc_device_type.cmake")
file(WRITE "${_mmc_patch_script}" [=[
if(NOT DEFINED INPUT_FILE)
  message(FATAL_ERROR "INPUT_FILE not provided")
endif()

if(NOT DEFINED TYPE)
  message(FATAL_ERROR "TYPE not provided (use SD or EMMC)")
endif()

if(NOT EXISTS "${INPUT_FILE}")
  message(FATAL_ERROR "File does not exist: ${INPUT_FILE}")
endif()

string(TOUPPER "${TYPE}" _type)

if(_type STREQUAL "EMMC")
  set(_mmc_val "0")
elseif(_type STREQUAL "SD")
  set(_mmc_val "1")
else()
  message(FATAL_ERROR "Invalid TYPE='${TYPE}'. Expected SD or EMMC")
endif()

file(READ "${INPUT_FILE}" _content)

# Replace any existing MMC_DEVICE_TYPE <num> to desired value
string(REGEX REPLACE
  "MMC_DEVICE_TYPE[ \t]*[0-9]+"
  "MMC_DEVICE_TYPE ${_mmc_val}"
  _content
  "${_content}"
)

file(WRITE "${INPUT_FILE}" "${_content}")

message(STATUS "Set MMC_DEVICE_TYPE=${_mmc_val} (${_type}) in: ${INPUT_FILE}")
]=])

#extract ATF platform from SOC
string(TOLOWER ${SOC} ATF_PLAT)

add_custom_command(
    OUTPUT ${ATF_BUILD_COPY_DIR}/atf.copy_done
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ATF_BUILD_COPY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ATF_SOURCE} ${ATF_BUILD_COPY_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${ATF_BUILD_COPY_DIR}/atf.copy_done
    COMMENT "Copying ATF to build directory"
)

add_custom_target(copy_atf
    DEPENDS ${ATF_BUILD_COPY_DIR}/atf.copy_done
)

if(NOT DEFINED ATF_LOG_LEVEL)
    set(ATF_LOG_LEVEL "25")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ATF_DEBUG_FLAG "1")
    set(ATF_BUILD_DIR "${ATF_BUILD_COPY_DIR}/build/${ATF_PLAT}/debug/")
else()
    set(ATF_DEBUG_FLAG "0")
    set(ATF_BUILD_DIR "${ATF_BUILD_COPY_DIR}/build/${ATF_PLAT}/release/")
endif()

message(STATUS "ATF Log Level : ${ATF_LOG_LEVEL}")

set(ATF_BUILD_CMD_QSPI_PRE
    make -C ${ATF_BUILD_COPY_DIR}
    CROSS_COMPILE=${CC_PREFIX}
    PLAT=${ATF_PLAT} SOCFPGA_BOOT_SOURCE_QSPI=1 bl2 bl31
    PRELOADED_BL33_BASE=0x82000000
    LOG_LEVEL=${ATF_LOG_LEVEL} DEBUG=${ATF_DEBUG_FLAG}
    -j${CMAKE_BUILD_PARALLEL_LEVEL}
)
set(ATF_BUILD_CMD_SD_PRE
    make -C ${ATF_BUILD_COPY_DIR}
    CROSS_COMPILE=${CC_PREFIX}
    PLAT=${ATF_PLAT} bl2 bl31
    PRELOADED_BL33_BASE=0x82000000
    LOG_LEVEL=${ATF_LOG_LEVEL} DEBUG=${ATF_DEBUG_FLAG}
    -j${CMAKE_BUILD_PARALLEL_LEVEL}
)
set(ATF_CLEAN_CMD_PRE
    make -C ${ATF_BUILD_COPY_DIR}
    PLAT=${ATF_PLAT}
    clean
)
set(ATF_BUILD_FIPTOOL_CMD_PRE
    make -C ${ATF_BUILD_COPY_DIR}
    fiptool
)

if(WIN32)
    string(JOIN " " ATF_BUILD_CMD_QSPI_PRE_STR ${ATF_BUILD_CMD_QSPI_PRE})
    string(JOIN " " ATF_BUILD_CMD_SD_PRE_STR ${ATF_BUILD_CMD_SD_PRE})
    string(JOIN " " ATF_CLEAN_CMD_PRE_STR ${ATF_CLEAN_CMD_PRE})
    string(JOIN " " ATF_BUILD_FIPTOOL_CMD_PRE_STR ${ATF_BUILD_FIPTOOL_CMD_PRE})
    set(ATF_BUILD_SCRIPT_QSPI "${CMAKE_BINARY_DIR}/build_atf_qspi.sh")
    set(ATF_BUILD_SCRIPT_SD "${CMAKE_BINARY_DIR}/build_atf_sd.sh")
    set(ATF_BUILD_FIPTOOL_SCRIPT "${CMAKE_BINARY_DIR}/build_atf_fiptool.sh")
    file(WRITE ${ATF_BUILD_SCRIPT_QSPI}
"#!/usr/bin/bash
export PATH=/opt/aarch-cc/bin:\$PATH
${ATF_CLEAN_CMD_PRE_STR}
${ATF_BUILD_CMD_QSPI_PRE_STR}
"
    )
    file(WRITE ${ATF_BUILD_SCRIPT_SD}
"#!/usr/bin/bash
export PATH=/opt/aarch-cc/bin:\$PATH
${ATF_CLEAN_CMD_PRE_STR}
${ATF_BUILD_CMD_SD_PRE_STR}
"
    )
    file(WRITE ${ATF_BUILD_FIPTOOL_SCRIPT}
"#!/usr/bin/bash
export PATH=/opt/aarch-cc/bin:\$PATH
${ATF_BUILD_FIPTOOL_CMD_PRE_STR}
"
    )

    format_mingw_command("${ATF_BUILD_SCRIPT_QSPI}" ATF_BUILD_CMD_QSPI)
    format_mingw_command("${ATF_BUILD_SCRIPT_SD}" ATF_BUILD_CMD_SD)
    format_mingw_command("${ATF_BUILD_FIPTOOL_SCRIPT}" ATF_BUILD_FIPTOOL_CMD)

    set(ATF_CLEAN_CMD
        ${CMAKE_COMMAND} -E
        echo
        "Building ATF"
    )
else()
    set(ATF_BUILD_CMD_QSPI ${ATF_BUILD_CMD_QSPI_PRE})
    set(ATF_BUILD_CMD_SD ${ATF_BUILD_CMD_SD_PRE})
    set(ATF_CLEAN_CMD ${ATF_CLEAN_CMD_PRE})
    set(ATF_BUILD_FIPTOOL_CMD ${ATF_BUILD_FIPTOOL_CMD_PRE})
endif()

# Add custom target to build ATF
add_custom_target(
    atf-bl2-bl31-qspi
    COMMAND ${ATF_CLEAN_CMD}
    COMMAND ${ATF_BUILD_CMD_QSPI}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${QSPI_ARTIFACTS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ATF_BUILD_DIR}/bl31.bin ${QSPI_ARTIFACTS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ATF_BUILD_DIR}/bl2.bin ${QSPI_ARTIFACTS_DIR}
    BYPRODUCTS ${QSPI_ARTIFACTS_DIR}/bl31.bin
    BYPRODUCTS ${QSPI_ARTIFACTS_DIR}/bl2.bin
    DEPENDS copy_atf
)

if(ATF_PLAT STREQUAL "agilex5")
    set(ATF_PLAT_DEF_FILE ${ATF_BUILD_COPY_DIR}/plat/intel/soc/agilex5/include/socfpga_plat_def.h)
else()
    set(ATF_PLAT_DEF_FILE ${ATF_BUILD_COPY_DIR}/plat/altera/soc/agilex3/include/socfpga_plat_def.h)
endif()

add_custom_target(
    atf-bl2-bl31-sd
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE="${ATF_PLAT_DEF_FILE}"
        -DTYPE="SD"
        -P "${_mmc_patch_script}"
    COMMAND ${ATF_CLEAN_CMD}
    COMMAND ${ATF_BUILD_CMD_SD}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SD_ARTIFACTS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ATF_BUILD_DIR}/bl31.bin ${SD_ARTIFACTS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ATF_BUILD_DIR}/bl2.bin ${SD_ARTIFACTS_DIR}
    BYPRODUCTS ${SD_ARTIFACTS_DIR}/bl31.bin
    BYPRODUCTS ${SD_ARTIFACTS_DIR}/bl2.bin
    DEPENDS copy_atf
)

add_custom_target(
    atf-bl2-bl31-emmc
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE="${ATF_PLAT_DEF_FILE}"
        -DTYPE="EMMC"
        -P "${_mmc_patch_script}"
    COMMAND ${ATF_CLEAN_CMD}
    COMMAND ${ATF_BUILD_CMD_SD}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EMMC_ARTIFACTS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ATF_BUILD_DIR}/bl31.bin ${EMMC_ARTIFACTS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ATF_BUILD_DIR}/bl2.bin ${EMMC_ARTIFACTS_DIR}
    BYPRODUCTS ${EMMC_ARTIFACTS_DIR}/bl31.bin
    BYPRODUCTS ${EMMC_ARTIFACTS_DIR}/bl2.bin
    DEPENDS copy_atf
)

add_custom_command(
    COMMAND ${ATF_BUILD_FIPTOOL_CMD}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ATF_BUILD_COPY_DIR}/tools/fiptool/fiptool ${CMAKE_BINARY_DIR}/fiptool
    OUTPUT ${CMAKE_BINARY_DIR}/fiptool
    VERBATIM
    )

add_custom_target(
    atf-fiptool
    DEPENDS ${CMAKE_BINARY_DIR}/fiptool
    DEPENDS copy_atf
)

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
        COMMAND ${CMAKE_OBJDUMP} -D ${target_name} >  ${ASM_FILE}
        COMMENT "Generating ${BIN_FILE} and ${ASM_FILE} from ${target_name}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/
    )
    add_custom_target(${BIN_FILE} DEPENDS ${target_name})
endfunction()

find_program(QUARTUS_PFG_EXECUTABLE
    NAMES quartus_pfg
    HINTS $ENV{QUARTUS_ROOTDIR}/bin
)

if(QUARTUS_PFG_EXECUTABLE)
    message(STATUS "Found quartus_pfg: ${QUARTUS_PFG_EXECUTABLE}")
    execute_process(
        COMMAND "${QUARTUS_PFG_EXECUTABLE}" --version
        RESULT_VARIABLE PFG_RESULT
        OUTPUT_VARIABLE PFG_VERSION
        ERROR_VARIABLE PFG_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    if(PFG_RESULT EQUAL 0)
        string(REGEX MATCH "Version ([0-9]+\\.[0-9]+\\.[0-9]+)" _match "${PFG_VERSION}")
        set(PFG_VERSION_NUMBER "${CMAKE_MATCH_1}")
        message(STATUS "quartus_pfg version: ${PFG_VERSION_NUMBER}")
    endif()
endif()

get_filename_component(SOCFPGA_SOF_FILE ${SOF_PATH} NAME)
set(SOCFPGA_SOF_FILE ${SOCFPGA_SOF_FILE} CACHE STRING "Name of the SOF file to be used for the FPGA")

add_custom_command(
    OUTPUT  "${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE}"
    COMMAND ${CMAKE_COMMAND} -E echo "Checking SOF file: ${SOF_PATH}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SOF_PATH}"
            "${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE}"
    DEPENDS "${SOF_PATH}"
    VERBATIM
)

add_custom_target(get_sof_file
    DEPENDS ${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE}
)

add_custom_target(get_sd_pfg_file
    COMMAND ${CMAKE_COMMAND} -E echo "Checking PFG_SDMMC file: ${PFG_SDMMC}"
    COMMAND ${CMAKE_COMMAND} -E compare_files "${PFG_SDMMC}" "${PFG_SDMMC}"
    DEPENDS "${PFG_SDMMC}"
    VERBATIM
)

add_custom_target(get_qspi_pfg_file
    COMMAND ${CMAKE_COMMAND} -E echo "Checking PFG_QSPI file: ${PFG_QSPI}"
    COMMAND ${CMAKE_COMMAND} -E compare_files "${PFG_QSPI}" "${PFG_QSPI}"
    DEPENDS "${PFG_QSPI}"
    VERBATIM
)

function(add_sd_image executable_name)
    if(executable_name MATCHES "\\.elf$")
        string(REGEX REPLACE "\\.elf$" ".bin" BIN_FILE "${executable_name}")
    else()
        set(BIN_FILE "${executable_name}.bin")
    endif()
    string(REGEX REPLACE ".bin" "" TARG_NAME ${BIN_FILE})
    set(FIP_FILE "fip.bin")
    set(FIPTOOL_BIN ${CMAKE_BINARY_DIR}/fiptool)
    set(SD_BL31_BIN ${SD_ARTIFACTS_DIR}/bl31.bin)
    set(SD_BL2_BIN ${SD_ARTIFACTS_DIR}/bl2.bin)

    set(EMMC_BL31_BIN ${EMMC_ARTIFACTS_DIR}/bl31.bin)
    set(EMMC_BL2_BIN ${EMMC_ARTIFACTS_DIR}/bl2.bin)

    add_custom_target(
        sd-image
        COMMAND ${FIPTOOL_BIN} create --soc-fw ${SD_BL31_BIN} --nt-fw ${CMAKE_CURRENT_BINARY_DIR}/${BIN_FILE} ${SD_ARTIFACTS_DIR}/${FIP_FILE}
        COMMAND rm -f sd.img
        COMMAND ${CMAKE_OBJCOPY} -v -I binary -O ihex --change-addresses 0x0 ${SD_BL2_BIN} bl2.hex
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE} ${SOCFPGA_SOF_FILE}
        COMMENT "Generating core.rbf from sof"
        COMMAND ${QUARTUS_PFG_EXECUTABLE} -c -o hps_path=bl2.hex ${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE} fsbl.sof
        COMMAND ${QUARTUS_PFG_EXECUTABLE} -c fsbl.sof ghrd.rbf -o hps=ON -o hps_path=bl2.hex
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fatfs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SD_ARTIFACTS_DIR}/ghrd.core.rbf ${CMAKE_BINARY_DIR}/fatfs/core.rbf
        COMMAND ${TOOL_DIR}/make_sdimage.sh -s 128 -o sd.img -p "a2:2:64:fip.bin" -p "c:1::../fatfs/core.rbf"
        COMMAND ${QUARTUS_PFG_EXECUTABLE} -c ${PFG_SDMMC}
        WORKING_DIRECTORY ${SD_ARTIFACTS_DIR}
        DEPENDS ${executable_name}
                ${BIN_FILE}
                get_sof_file
                get_sd_pfg_file
                atf-bl2-bl31-sd
                atf-fiptool
                ${QUARTUS_PFG_EXECUTABLE}
        BYPRODUCTS sd.img fsbl.sof
    )

    add_custom_target(
        emmc-image
        COMMAND ${FIPTOOL_BIN} create --soc-fw ${EMMC_BL31_BIN} --nt-fw ${CMAKE_CURRENT_BINARY_DIR}/${BIN_FILE} ${EMMC_ARTIFACTS_DIR}/${FIP_FILE}
        COMMAND rm -f sd_emmc.img
        COMMAND ${CMAKE_OBJCOPY} -v -I binary -O ihex --change-addresses 0x0 ${EMMC_BL2_BIN} bl2.hex
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE} ${SOCFPGA_SOF_FILE}
        COMMENT "Generating core.rbf from sof"
        COMMAND ${QUARTUS_PFG_EXECUTABLE} -c -o hps_path=bl2.hex ${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE} fsbl.sof
        COMMAND ${QUARTUS_PFG_EXECUTABLE} -c fsbl.sof ghrd.rbf -o hps=ON -o hps_path=bl2.hex
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fatfs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EMMC_ARTIFACTS_DIR}/ghrd.core.rbf ${CMAKE_BINARY_DIR}/fatfs/core.rbf
        COMMAND ${TOOL_DIR}/make_sdimage.sh -s 128 -o sd_emmc.img -p "a2:2:64:fip.bin" -p "c:1::../fatfs/core.rbf"
        COMMAND ${QUARTUS_PFG_EXECUTABLE} -c ${PFG_SDMMC}
        WORKING_DIRECTORY ${EMMC_ARTIFACTS_DIR}
        DEPENDS ${executable_name}
                ${BIN_FILE}
                get_sof_file
                get_sd_pfg_file
                atf-bl2-bl31-emmc
                atf-fiptool
                ${QUARTUS_PFG_EXECUTABLE}
        BYPRODUCTS sd_emmc.img
    )
endfunction()

function(add_qspi_image executable_name)
    if(executable_name MATCHES "\\.elf$")
        string(REGEX REPLACE "\\.elf$" ".bin" BIN_FILE "${executable_name}")
    else()
        set(BIN_FILE "${executable_name}.bin")
    endif()
    string(REGEX REPLACE ".bin" "" TARG_NAME ${BIN_FILE})
    set(FIP_FILE "fip.bin")
    set(FIPTOOL_BIN ${CMAKE_BINARY_DIR}/fiptool)
    set(BL31_BIN ${QSPI_ARTIFACTS_DIR}/bl31.bin)
    set(BL2_BIN ${QSPI_ARTIFACTS_DIR}/bl2.bin)

    add_custom_target(
        qspi-image
        COMMAND ${CMAKE_COMMAND} -E echo "${Yellow}Using pfg file : ${PFG_QSPI} ${ColourReset}"
        COMMAND ${CMAKE_COMMAND} -E echo "${Yellow}Using sof file : ${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE} ${ColourReset}"
        COMMAND ${FIPTOOL_BIN} create --soc-fw ${BL31_BIN} --nt-fw ${CMAKE_CURRENT_BINARY_DIR}/${BIN_FILE} ${QSPI_ARTIFACTS_DIR}/${FIP_FILE}
        COMMAND ${CMAKE_OBJCOPY} -I binary -O ihex --change-addresses 0x0 ${BL2_BIN} bl2.hex
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE} ${QSPI_ARTIFACTS_DIR}/${SOCFPGA_SOF_FILE}
        COMMAND ${QUARTUS_PFG_EXECUTABLE} -c ${PFG_QSPI}
        COMMENT "Generating core.rbf from sof"
        COMMAND ${QUARTUS_PFG_EXECUTABLE} -c -o hps_path=bl2.hex ${CMAKE_BINARY_DIR}/${SOCFPGA_SOF_FILE} fsbl.sof
        COMMAND ${QUARTUS_PFG_EXECUTABLE} -c fsbl.sof ghrd.rbf -o hps=ON -o hps_path=bl2.hex
        WORKING_DIRECTORY ${QSPI_ARTIFACTS_DIR}
        DEPENDS ${executable_name}
                ${BIN_FILE}
                get_sof_file
                get_qspi_pfg_file
                atf-bl2-bl31-qspi
                atf-fiptool
                ${QUARTUS_PFG_EXECUTABLE}
        BYPRODUCTS qspi_image.jic
    )
endfunction()
