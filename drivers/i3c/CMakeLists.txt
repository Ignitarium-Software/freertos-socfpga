target_sources(socfpga_drivers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_i3c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_i3c_ll.c
    )

 target_include_directories(socfpga_drivers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
