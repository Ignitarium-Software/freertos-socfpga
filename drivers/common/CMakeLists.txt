target_sources(socfpga_drivers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_cache_maintenance.S
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_platform.c
    )

set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_cache_maintenance.S
    DIRECTORY ${CMAKE_SOURCE_DIR}
    PROPERTY LANGUAGE ASM
    )

target_include_directories(socfpga_drivers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
