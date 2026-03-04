
add_library(freertos_kernel_port OBJECT)
add_dependencies(freertos_kernel_port apply_smp_patch)
target_sources(freertos_kernel_port
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/portmacro.h
    ${CMAKE_CURRENT_SOURCE_DIR}/portASM.S
    ${CMAKE_CURRENT_SOURCE_DIR}/port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/portSocfpga.c
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_3_extra.c
    ${CMAKE_CURRENT_SOURCE_DIR}/portStandardLib.c
    ${FREERTOS_TOP_DIR}/FreeRTOS/Demo/SOCFPGA/startup/cpu_init.S
    ${FREERTOS_TOP_DIR}/FreeRTOS/Demo/SOCFPGA/startup/setup_pagetable.c
    ${FREERTOS_TOP_DIR}/drivers/console/socfpga_console.c
    ${FREERTOS_TOP_DIR}/drivers/uart/socfpga_uart.c
    ${FREERTOS_TOP_DIR}/drivers/uart/socfpga_uart_ll.c
    ${FREERTOS_TOP_DIR}/drivers/sdm_mailbox/socfpga_sip_handler.c
    ${FREERTOS_TOP_DIR}/drivers/reset_mngr/socfpga_rst_mngr.c
    ${FREERTOS_TOP_DIR}/drivers/clk_mngr/socfpga_clk_mngr.c
)

set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/portASM.S
    DIRECTORY ${CMAKE_SOURCE_DIR}
    PROPERTY LANGUAGE ASM
    )

add_library(freertos_kernel_port_headers INTERFACE)
target_include_directories(freertos_kernel_port_headers
    INTERFACE
    .
    ${FREERTOS_TOP_DIR}/drivers/gic/
    ${FREERTOS_TOP_DIR}/drivers/console/
    ${FREERTOS_TOP_DIR}/drivers/common/
    ${FREERTOS_TOP_DIR}/drivers/uart/
    ${FREERTOS_TOP_DIR}/drivers/sdm_mailbox/
    ${FREERTOS_TOP_DIR}/drivers/clk_mngr/
    ${FREERTOS_TOP_DIR}/drivers/sys_mngr/
    ${FREERTOS_TOP_DIR}/drivers/reset_mngr/
    ${FREERTOS_TOP_DIR}/osal/freertos/
    ${FREERTOS_TOP_DIR}/FreeRTOS/FreeRTOS-Plus/Source/Utilities/logging/
    )
target_link_libraries(freertos_kernel_port
    PRIVATE
    freertos_kernel_port_headers
    freertos_kernel_include
    )
