
add_library(freertos_kernel_port OBJECT)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(freertos_kernel_port PRIVATE ${DEFAULT_C_FLAGS_DEBUG})
else()
    target_compile_options(freertos_kernel_port PRIVATE ${DEFAULT_C_FLAGS_RELEASE})
endif()

add_dependencies(freertos_kernel_port apply_smp_patch)
target_sources(freertos_kernel_port
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/portmacro.h
    ${CMAKE_CURRENT_SOURCE_DIR}/portASM.S
    ${CMAKE_CURRENT_SOURCE_DIR}/port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/portSocfpga.c
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_3_extra.c
    # portStandardLib.c is intentionally omitted here: it calls console driver
    # functions (console_read/fill/signal) that live in socfpga_drivers, which is
    # archived into libfreertos_socfpga.a. Placing it in freertos_kernel_port
    # causes it to land in libfreertos_kernel.a, which is linked after
    # libfreertos_socfpga.a. The GNU linker's single left-to-right pass over
    # archives cannot resolve back-references, so those symbols are undefined at
    # link time. The file is compiled as part of freertos_socfpga instead, where
    # it shares an archive with socfpga_console.c and the dependency is satisfied
    # within a single archive pass.
    ${FREERTOS_TOP_DIR}/FreeRTOS/Demo/SOCFPGA/startup/cpu_init.S
    ${FREERTOS_TOP_DIR}/FreeRTOS/Demo/SOCFPGA/startup/setup_pagetable.c
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
