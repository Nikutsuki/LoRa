# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.12)

set(PICO_BOARD pico CACHE STRING "Board type")

# pico_sdk_import.cmake is a single file copied from this SDK.
# Note: this must happen before project().
include(pico_sdk_import.cmake)

project(pico_lora_sx1262 CXX C ASM)

pico_sdk_init()

set(LORAMAC_NODE_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/LoRaMac-node)

# ------------------------------------------------------------------------------------
# Semtech LoRaMac-node: SX126x radio driver only.
#
# Upstream is a full LoRaWAN stack of roughly 1600 files. Only the driver and the
# system shims it needs are vendored here, so this is a point-to-point radio
# rather than a MAC layer. The source list below is the complete transitive
# closure of what those files include; extend it if you enable more of upstream.
# ------------------------------------------------------------------------------------
add_library(pico_loramac_node INTERFACE)

target_sources(pico_loramac_node INTERFACE
    ${LORAMAC_NODE_PATH}/src/boards/mcu/utilities.c

    ${LORAMAC_NODE_PATH}/src/radio/sx126x/sx126x.c
    ${LORAMAC_NODE_PATH}/src/radio/sx126x/radio.c

    ${LORAMAC_NODE_PATH}/src/system/delay.c
    ${LORAMAC_NODE_PATH}/src/system/fifo.c
    ${LORAMAC_NODE_PATH}/src/system/gpio.c
    ${LORAMAC_NODE_PATH}/src/system/nvmm.c
    ${LORAMAC_NODE_PATH}/src/system/systime.c
    ${LORAMAC_NODE_PATH}/src/system/timer.c

    # Board glue adapting the driver onto RP2040 peripherals.
    ${CMAKE_CURRENT_LIST_DIR}/src/board/board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/board/delay-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/board/eeprom-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/board/gpio-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/board/rtc-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/board/spi-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/board/sx1262-board.c
)

target_include_directories(pico_loramac_node INTERFACE
    ${LORAMAC_NODE_PATH}/src
    ${LORAMAC_NODE_PATH}/src/boards
    ${LORAMAC_NODE_PATH}/src/radio
    ${LORAMAC_NODE_PATH}/src/radio/sx126x
    ${LORAMAC_NODE_PATH}/src/system
    ${CMAKE_CURRENT_LIST_DIR}/src/board
)

target_link_libraries(pico_loramac_node INTERFACE pico_stdlib pico_unique_id hardware_spi)

# ------------------------------------------------------------------------------------
# Application
# ------------------------------------------------------------------------------------
add_executable(pico_lora_messenger
    src/main.cpp
    src/HardwareTestApp.cpp
    src/Ili9341Display.cpp
    src/CardKbKeyboard.cpp
    src/CryptoBox.cpp
    src/FileUploadManager.cpp
    src/InviteProtocol.cpp
    src/ContactStore.cpp
    src/ChatHistory.cpp
    src/LoraMessenger.cpp

    src/ui/Ui.cpp
    src/ui/ScreenModel.cpp
    src/ui/LcdScreen.cpp

    src/protocol/Framing.cpp
    src/protocol/Messages.cpp
    src/protocol/LinkSession.cpp
    src/protocol/UsbCdcTransport.cpp
    src/protocol/UartTransport.cpp

    src/adapters/UartScreen.cpp
    src/adapters/UartInput.cpp
    src/adapters/CardKbInput.cpp

    ${CMAKE_CURRENT_LIST_DIR}/lib/monocypher/monocypher.c
)

target_include_directories(pico_lora_messenger PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/lib/font8x8
    ${CMAKE_CURRENT_LIST_DIR}/lib/monocypher
)

target_compile_definitions(pico_lora_messenger PRIVATE
    USE_MODEM_LORA
    REGION_EU433
)

target_link_libraries(pico_lora_messenger
    pico_loramac_node
    hardware_i2c
    hardware_flash
    hardware_spi
    hardware_uart
    pico_stdlib
)

# The desktop UI talks to this firmware over the USB CDC port, so enable USB
# stdio and leave the hardware UART for the optional link transport.
pico_enable_stdio_usb(pico_lora_messenger 1)
pico_enable_stdio_uart(pico_lora_messenger 0)

pico_add_extra_outputs(pico_lora_messenger)
