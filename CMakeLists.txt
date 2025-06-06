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
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.12)

# initialize pico_sdk from GIT
# (note this can come from environment, CMake cache etc)
# set(PICO_SDK_FETCH_FROM_GIT on)

# pico_sdk_import.cmake is a single file copied from this SDK
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(pico_lora_sx1262)

# initialize the Pico SDK
pico_sdk_init()

set(LORAMAC_NODE_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/LoRaMac-node)

add_library(pico_loramac_node INTERFACE)

target_sources(pico_loramac_node INTERFACE
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/CayenneLpp.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/cli.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/LmHandlerMsgDisplay.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/NvmDataMgmt.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/LmHandler/LmHandler.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/LmHandler/packages/FragDecoder.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/LmHandler/packages/LmhpClockSync.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/LmHandler/packages/LmhpCompliance.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/LmHandler/packages/LmhpFragmentation.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/LmHandler/packages/LmhpRemoteMcastSetup.c
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/NvmDataMgmt.c

    ${LORAMAC_NODE_PATH}/src/boards/mcu/utilities.c

    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/NvmDataMgmt.c

    # ${LORAMAC_NODE_PATH}/src/mac/region/Region.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionAS923.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionAU915.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionBaseUS.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionCN470.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionCN470A20.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionCN470A26.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionCN470B20.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionCN470B26.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionCN779.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionCommon.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionEU433.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionEU868.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionIN865.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionKR920.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionRU864.c
    # ${LORAMAC_NODE_PATH}/src/mac/region/RegionUS915.c
    # ${LORAMAC_NODE_PATH}/src/mac/LoRaMac.c
    # ${LORAMAC_NODE_PATH}/src/mac/LoRaMacAdr.c
    # ${LORAMAC_NODE_PATH}/src/mac/LoRaMacClassB.c
    # ${LORAMAC_NODE_PATH}/src/mac/LoRaMacCommands.c
    # ${LORAMAC_NODE_PATH}/src/mac/LoRaMacConfirmQueue.c
    # ${LORAMAC_NODE_PATH}/src/mac/LoRaMacCrypto.c
    # ${LORAMAC_NODE_PATH}/src/mac/LoRaMacParser.c
    # ${LORAMAC_NODE_PATH}/src/mac/LoRaMacSerializer.c

    # ${LORAMAC_NODE_PATH}/src/peripherals/soft-se/aes.c
    # ${LORAMAC_NODE_PATH}/src/peripherals/soft-se/cmac.c
    # ${LORAMAC_NODE_PATH}/src/peripherals/soft-se/soft-se-hal.c
    # ${LORAMAC_NODE_PATH}/src/peripherals/soft-se/soft-se.c
    
    ${LORAMAC_NODE_PATH}/src/radio/sx126x/sx126x.c
    ${LORAMAC_NODE_PATH}/src/radio/sx126x/radio.c
    # ${LORAMAC_NODE_PATH}/src/radio/sx1276/sx1276.c

    ${LORAMAC_NODE_PATH}/src/system/delay.c
    ${LORAMAC_NODE_PATH}/src/system/gpio.c
    ${LORAMAC_NODE_PATH}/src/system/nvmm.c
    ${LORAMAC_NODE_PATH}/src/system/systime.c
    ${LORAMAC_NODE_PATH}/src/system/timer.c
    ${LORAMAC_NODE_PATH}/src/system/fifo.c

    ${CMAKE_CURRENT_LIST_DIR}/src/board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/delay-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/eeprom-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/gpio-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rtc-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/spi-board.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sx1262-board.c
    # ${CMAKE_CURRENT_LIST_DIR}/src/boards/rp2040/sx1276-board.c


    

)

target_include_directories(pico_loramac_node INTERFACE
    ${LORAMAC_NODE_PATH}/src
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/LmHandler
    # ${LORAMAC_NODE_PATH}/src/apps/LoRaMac/common/LmHandler/packages
    ${LORAMAC_NODE_PATH}/src/boards
    # ${LORAMAC_NODE_PATH}/src/mac
    # ${LORAMAC_NODE_PATH}/src/mac/region
    # ${LORAMAC_NODE_PATH}/src/peripherals/soft-se
    ${LORAMAC_NODE_PATH}/src/radio
    ${LORAMAC_NODE_PATH}/src/radio/sx126x
    ${LORAMAC_NODE_PATH}/src/system
    ${LORAMAC_NODE_PATH}/src/peripherals
)

target_link_libraries(pico_loramac_node INTERFACE pico_stdlib pico_unique_id hardware_spi)

# target_compile_definitions(pico_loramac_node INTERFACE -DSOFT_SE)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_EU868)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_US915)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_CN779)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_EU433)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_AU915)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_AS923)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_CN470)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_KR920)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_IN865)
# target_compile_definitions(pico_loramac_node INTERFACE -DREGION_RU864)
# target_compile_definitions(pico_loramac_node INTERFACE -DACTIVE_REGION=LORAMAC_REGION_US915)

add_library(pico_lora_sx1262 INTERFACE)

# target_sources(pico_lora_sx1262 INTERFACE
#     ${CMAKE_CURRENT_LIST_DIR}/src/lorawan.c
# )

# target_include_directories(pico_lora_sx1262 INTERFACE
#     ${CMAKE_CURRENT_LIST_DIR}/src/include
# )

target_link_libraries(pico_lora_sx1262 INTERFACE pico_loramac_node)

# enable usb output, disable uart output
# pico_enable_stdio_usb(pico_lora_sx1262 1)
# pico_enable_stdio_uart(pico_lora_sx1262 0)

add_subdirectory("examples/pico-ping-pong")
# add_subdirectory("examples/default_dev_eui")
# add_subdirectory("examples/hello_abp")
# add_subdirectory("examples/hello_otaa")
# add_subdirectory("examples/otaa_temperature_led")
