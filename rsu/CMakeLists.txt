set(SHARED_LIB OFF)
set(FREERTOS_TOOLCHAIN TOOLCHAIN)
set(PLATFORM freertos)


add_subdirectory(librsu)
target_compile_options(uniLibRSU PRIVATE
  -march=armv8-a+crc
  -Wno-format
  -Wno-format-security
  -Wno-error=format-security
)

add_subdirectory(portable)
target_link_libraries(uniLibRSU freertos_kernel socfpga_drivers)

