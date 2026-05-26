cmake_minimum_required(VERSION 3.5...3.28)
project(rsu_sample NONE)

set(RSU_TYPE "rsu_sdcard" CACHE STRING "RSU sample type: rsu_ddr | rsu_qspi | rsu_sdcard")
set_property(CACHE RSU_TYPE PROPERTY STRINGS rsu_ddr rsu_qspi rsu_sdcard)

set(_valid_rsu_types rsu_ddr rsu_qspi rsu_sdcard)
if(NOT RSU_TYPE IN_LIST _valid_rsu_types)
    message(FATAL_ERROR "RSU_TYPE must be one of: ${_valid_rsu_types}. Got: '${RSU_TYPE}'")
endif()

# -DPFG_QSPI=<path> overrides the RSU initial image PFG used by the child cmake.
# The child's default is its own initial_image.pfg; pass this to override it.
if(DEFINED PFG_QSPI AND NOT PFG_QSPI STREQUAL "")
    set(PFG_PATH "${PFG_QSPI}" CACHE STRING "RSU initial image PFG path" FORCE)
endif()

add_subdirectory(${RSU_TYPE})
