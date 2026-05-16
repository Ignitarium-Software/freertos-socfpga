target_sources(socfpga_drivers PRIVATE
    arm_gic.c
    arm_gic_reg.c
    socfpga_interrupt.c
    socfpga_interrupt.h
    )

target_include_directories(socfpga_drivers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
