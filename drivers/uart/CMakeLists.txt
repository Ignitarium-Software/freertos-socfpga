target_sources(socfpga_drivers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/socfpga_uart_ll.c
    )

target_include_directories(socfpga_drivers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
