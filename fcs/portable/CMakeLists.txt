cmake_minimum_required(VERSION 3.24)

target_sources(FCS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libfcs_logging.c")
target_sources(FCS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libfcs_file_ops.c")
target_sources(FCS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libfcs_osal.c")
target_sources(FCS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libfcs_utils.c")

target_include_directories(FCS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
