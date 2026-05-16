cmake_minimum_required(VERSION 3.5...3.28)

# Set SOF Path
if(NOT DEFINED SOF_PATH)
    set(SOF_PATH "${CMAKE_SOURCE_DIR}/ghrd_a5ed065bb32ae6sr0.sof" CACHE STRING "SOF File path" FORCE)
    if(SOC STREQUAL "AGILEX3")
        set(SOF_PATH "${CMAKE_SOURCE_DIR}/ghrd_a3cw135bm16ae6s.sof" CACHE STRING "SOF File path" FORCE)
    endif()
endif()

# Download PFG into build dir
# ===============================

set(PFG_URL
    "https://releases.rocketboards.org/2024.11/zephyr/agilex5/hps_zephyr/hello_world/qspi_boot/qspi_flash_image_agilex5_boot.pfg"
)

# Default values for when -DSOC parameter is not used
set(SOF_FILE_SEARCH_WORD "a3cw135bm16ae6s")
set(SOF_FILE_REPLACE_WORD "a5ed065bb32ae6sr0")

if(SOC STREQUAL "AGILEX3")
    set(SOF_FILE_SEARCH_WORD "a5ed065bb32ae6sr0")
    set(SOF_FILE_REPLACE_WORD "a3cw135bm16ae6s")
endif()

if(NOT DEFINED PFG_QSPI)
    set(PFG_QSPI
        "${CMAKE_BINARY_DIR}/qspi_flash_image_agilex5_boot.pfg"
        CACHE STRING "PFG File path" FORCE
    )
    if(NOT EXISTS "${PFG_QSPI}")
        message(STATUS "Downloading PFG from ${PFG_URL}")
        file(DOWNLOAD
            ${PFG_URL}
            ${PFG_QSPI}
            STATUS download_status
            SHOW_PROGRESS
        )
        list(GET download_status 0 status_code)
        list(GET download_status 1 status_string)
        if(NOT status_code EQUAL 0)
            message(FATAL_ERROR "Failed to download PFG: ${status_string}")
        endif()
    endif()
    file(READ "${PFG_QSPI}" pfg_content)
    string(REPLACE
        ${SOF_FILE_SEARCH_WORD}
        ${SOF_FILE_REPLACE_WORD}
        pfg_content
        "${pfg_content}"
    )
    file(WRITE "${PFG_QSPI}" "${pfg_content}")
else()
    if(NOT EXISTS "${PFG_QSPI}")
        message(FATAL_ERROR "PFG_QSPI not found: ${PFG_QSPI}")
    endif()
endif()

if(NOT DEFINED PFG_SDMMC)
    set(PFG_SDMMC
        "${CMAKE_BINARY_DIR}/qspi_flash_image_agilex5_sdmmc.pfg"
        CACHE STRING "SD/MMC PFG FIle path" FORCE
    )
else()
    if(NOT EXISTS "${PFG_SDMMC}")
        message(FATAL_ERROR "PFG_SDMMC not found: ${PFG_SDMMC}")
    endif()
endif()

# Remove FPI image from the PFG file (SD and MMC dosent need this)
if(NOT EXISTS "${PFG_QSPI}")
    message(FATAL_ERROR "PFG_QSPI not found: ${PFG_QSPI}")
endif()
file(READ "${PFG_QSPI}" PFG_TEXT)
string(REGEX REPLACE
    "[ \t\r\n]*<raw_files>[ \t\r\n]*<raw_file[^>]*id=\"Raw_File_1\"[^>]*>fip\\.bin</raw_file>[ \t\r\n]*</raw_files>[ \t\r\n]*"
    "\n"
    PFG_TEXT
    "${PFG_TEXT}"
)
string(REGEX REPLACE
    "[ \t\r\n]*<partition[^>]*id=\"fip\"[^>]*/>[ \t\r\n]*"
    "\n"
    PFG_TEXT
    "${PFG_TEXT}"
)
string(REGEX REPLACE
    "[ \t\r\n]*<assignment[^>]*partition_id=\"fip\"[^>]*>[ \t\r\n]*<raw_file_id>[ \t\r\n]*Raw_File_1[ \t\r\n]*</raw_file_id>[ \t\r\n]*</assignment>[ \t\r\n]*"
    "\n"
    PFG_TEXT
    "${PFG_TEXT}"
)
file(WRITE "${PFG_SDMMC}" "${PFG_TEXT}")

file(READ "${PFG_SDMMC}" pfg_content)
string(REPLACE
    ${SOF_FILE_SEARCH_WORD}
    ${SOF_FILE_REPLACE_WORD}
    pfg_content
    "${pfg_content}"
)
file(WRITE "${PFG_SDMMC}" "${pfg_content}")
