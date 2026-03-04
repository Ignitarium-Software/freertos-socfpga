cmake_minimum_required(VERSION 3.15.0)

target_include_directories(freertos_socfpga
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Plus/Source/Utilities/logging/
)

if (FREERTOS_CLI )
    file(GLOB CLI_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-CLI/*.c)
    target_include_directories(freertos_socfpga
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-CLI
    )
endif()

#Build Custom port for socfpga
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/portable)

#Build FreeRTOS-Kernel
add_library(freertos_config INTERFACE)

if(SMP)
    target_compile_definitions(freertos_config INTERFACE SMP=1)
else()
    target_compile_definitions(freertos_config INTERFACE SMP=0)
endif()
target_include_directories(freertos_config
    SYSTEM
    INTERFACE
    ${FREERTOS_CONFIG_HEADER_PATH}
)
target_compile_definitions(freertos_config
  INTERFACE
  projCOVERAGE_TEST=0
)

#set freertos heap to use
set( FREERTOS_HEAP "3" CACHE STRING "" FORCE)
set( FREERTOS_PORT "A_CUSTOM_PORT" CACHE STRING "" FORCE)
add_subdirectory(Source)

if (FREERTOS_TCPIP)
    set(FREERTOS_PLUS_TCP_NETWORK_IF "A_CUSTOM_NETWORK_IF" CACHE STRING "" FORCE)
    set(FREERTOS_PLUS_TCP_BUFFER_ALLOCATION "1" CACHE STRING "" FORCE)

    #Build TCP-IP stack
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-TCP)

    #Build portable layer for TCP stack
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Plus/Source/portable/NetworkInterface/SOCFPGA/)

    target_link_libraries(freertos_socfpga freertos_plus_tcp_network_if)
endif()

set(MAIN_SOURCE_SRCS
    ${CLI_SRCS}
    PARENT_SCOPE
    )
