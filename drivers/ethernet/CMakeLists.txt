add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/phy)

target_sources(socfpga_drivers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_xgmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_xgmac_ll.c
    )

target_include_directories(socfpga_drivers PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        )
