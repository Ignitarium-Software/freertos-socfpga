target_sources(socfpga_drivers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_phy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_phy_mdio.c
    )
if(SOC STREQUAL "AGILEX5")
    target_sources(socfpga_drivers PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/phy_marvell_88e1512.c
        )
elseif(SOC STREQUAL "AGILEX3")
    target_sources(socfpga_drivers PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/phy_microchip_ksz9031rnx.c
        )
endif()

target_include_directories(socfpga_drivers PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        )
