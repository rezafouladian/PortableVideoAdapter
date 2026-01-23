cmake_minimum_required(VERSION 3.12)

set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/external)
set(PICO_BOARD waveshare_rp2350_pizero)

include(external/picodvi/software/pico_sdk_import.cmake)

project(PortableVideoAdapter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)



pico_sdk_init()

add_compile_options(-Wall)

include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/external/picodvi/software/include
)

add_subdirectory(external/picodvi/software/libdvi)

add_executable(PortableVideoAdapter
        main.c
)

pico_generate_pio_header(PortableVideoAdapter
        ${CMAKE_CURRENT_LIST_DIR}/overrides/dvi_serialiser.pio
)

target_include_directories(PortableVideoAdapter PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/external/picodvi/software/include
)

target_compile_options(PortableVideoAdapter PRIVATE -Wall)

pico_enable_stdio_uart(PortableVideoAdapter 1)
pico_enable_stdio_usb(PortableVideoAdapter 0)

target_link_libraries(PortableVideoAdapter
        pico_stdlib
        pico_multicore
        pico_util
        libdvi
)

target_compile_definitions(PortableVideoAdapter PRIVATE
        DVI_DEFAULT_SERIAL_CONFIG=waveshare_rp2350_pizero
        DVI_VERTICAL_REPEAT=1
        DVI_N_TMDS_BUFFERS=3
        PICO_PIO_USE_GPIO_BASE=1
        DVI_1BPP_BIT_REVERSE=0
)

pico_add_extra_outputs(PortableVideoAdapter)
