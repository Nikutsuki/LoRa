cmake_minimum_required(VERSION 3.12)

add_executable(pico_hw_test
    main.cpp
    HardwareTestApp.cpp
    Ui.cpp
    Ili9341Display.cpp
    CardKbKeyboard.cpp
    CryptoBox.cpp
    InviteProtocol.cpp
    ContactStore.cpp
    LoraMessenger.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/monocypher/monocypher.c
)

target_include_directories(pico_hw_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/font8x8
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/monocypher
)

target_compile_definitions(pico_hw_test PRIVATE
    USE_MODEM_LORA
    REGION_EU433
)

target_link_libraries(pico_hw_test
    pico_lora_sx1262
    hardware_i2c
    hardware_flash
    hardware_spi
    pico_stdlib
)

pico_enable_stdio_usb(pico_hw_test 1)
pico_enable_stdio_uart(pico_hw_test 0)

pico_add_extra_outputs(pico_hw_test)
