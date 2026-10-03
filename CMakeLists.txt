cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME standard_robot)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # application
    ${CMAKE_CURRENT_SOURCE_DIR}/application/calibrate_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/CAN_receive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/chassis_behaviour.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/chassis_power_control.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/chassis_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/detect_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/gimbal_behaviour.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/gimbal_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/INS_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/led_flow_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/oled_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/referee_usart_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/referee.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/remote_control.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/servo_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/shoot.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/test_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/usb_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/application/voltage_task.c

    # bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_buzzer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_can.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_crc32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_delay.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_fric.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_imu_pwm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_laser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_rc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_rng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_servo_pwm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_spi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards/bsp_usart.c

    # components (arm_math_gcc_shim.c replaces the Keil-only CMSIS-DSP archive)
    ${CMAKE_CURRENT_SOURCE_DIR}/components/algorithm/AHRS_middleware.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/algorithm/arm_math_gcc_shim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/algorithm/user_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/controller/pid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/devices/BMI088driver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/devices/BMI088Middleware.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/devices/ist8310driver_middleware.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/devices/ist8310driver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/devices/OLED.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/support/CRC8_CRC16.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/support/fifo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/components/support/mem_mang4.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/application
    ${CMAKE_CURRENT_SOURCE_DIR}/application/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/boards
    ${CMAKE_CURRENT_SOURCE_DIR}/components/algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/components/algorithm/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/components/controller
    ${CMAKE_CURRENT_SOURCE_DIR}/components/devices
    ${CMAKE_CURRENT_SOURCE_DIR}/components/support
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    ARM_MATH_CM4
    ARM_MATH_MATRIX_CHECK
    ARM_MATH_ROUNDING
    __FPU_PRESENT=1U
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Prebuilt AHRS library shipped with the project (GNU ARM EABI, hard-float)
    ${CMAKE_CURRENT_SOURCE_DIR}/components/algorithm/AHRS.lib
)
