# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

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
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(electricity_vending_machine C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(electricity_vending_machine src/electricity_vending_machine.c
                                           OLED_SH1106_I2C/SH1106_I2C.c
                                           OLED_SH1106_I2C/FONTS/FONT_spleen_8x16.c
                                           PZEM-004T_UART_Pico/src/pzem-004t_modbus-rtu.c )

pico_set_program_name(electricity_vending_machine "electricity_vending_machine")
pico_set_program_version(electricity_vending_machine "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(electricity_vending_machine 0)
pico_enable_stdio_usb(electricity_vending_machine 1)

# Add the standard library to the build
target_link_libraries(electricity_vending_machine
        pico_stdlib
        pico_multicore
        pico_time)

# Add the standard include files to the build
target_include_directories(electricity_vending_machine PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/OLED_SH1106_I2C
        ${CMAKE_CURRENT_LIST_DIR}/OLED_SH1106_I2C/FONTS
        ${CMAKE_CURRENT_LIST_DIR}/PZEM-004T_UART_Pico/include
)

# Add any user requested libraries
target_link_libraries(electricity_vending_machine 
        hardware_i2c
        
        )

pico_add_extra_outputs(electricity_vending_machine)

