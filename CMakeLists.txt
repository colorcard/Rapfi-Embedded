cmake_minimum_required(VERSION 3.22)

#
# STM32G474VET6_LCD —— 逐飞风格开源库工程
#
# 目录结构：
#   Drivers/            STM32 HAL / CMSIS（厂商底层）
#   libraries/sdk/      system、HAL 配置、newlib 桩
#   libraries/zf_common 公共层（时钟、延时、BSP 入口、中断）
#   libraries/zf_driver 芯片外设驱动层
#   libraries/zf_device 外接设备驱动层
#   project/user        框架入口（main / isr）
#   project/code        用户业务代码
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
set(CMAKE_PROJECT_NAME STM32G474VET6_LCD)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# ---------------------------------------------------------------------------
# 厂商 HAL / CMSIS 源码
# ---------------------------------------------------------------------------
set(HAL_DIR ${CMAKE_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Src)

# ---------------------------------------------------------------------------
# 源文件
# ---------------------------------------------------------------------------
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # 启动文件与厂商底层
    ${CMAKE_SOURCE_DIR}/startup_stm32g474xx.s
    ${CMAKE_SOURCE_DIR}/libraries/sdk/system_stm32g4xx.c
    ${CMAKE_SOURCE_DIR}/libraries/sdk/syscalls.c
    ${CMAKE_SOURCE_DIR}/libraries/sdk/sysmem.c

    # HAL 驱动
    ${HAL_DIR}/stm32g4xx_hal.c
    ${HAL_DIR}/stm32g4xx_hal_cortex.c
    ${HAL_DIR}/stm32g4xx_hal_dma.c
    ${HAL_DIR}/stm32g4xx_hal_dma_ex.c
    ${HAL_DIR}/stm32g4xx_hal_exti.c
    ${HAL_DIR}/stm32g4xx_hal_flash.c
    ${HAL_DIR}/stm32g4xx_hal_flash_ex.c
    ${HAL_DIR}/stm32g4xx_hal_flash_ramfunc.c
    ${HAL_DIR}/stm32g4xx_hal_gpio.c
    ${HAL_DIR}/stm32g4xx_hal_pwr.c
    ${HAL_DIR}/stm32g4xx_hal_pwr_ex.c
    ${HAL_DIR}/stm32g4xx_hal_rcc.c
    ${HAL_DIR}/stm32g4xx_hal_rcc_ex.c
    ${HAL_DIR}/stm32g4xx_hal_adc.c
    ${HAL_DIR}/stm32g4xx_hal_adc_ex.c
    ${HAL_DIR}/stm32g4xx_ll_adc.c
    ${HAL_DIR}/stm32g4xx_hal_fdcan.c
    ${HAL_DIR}/stm32g4xx_hal_i2c.c
    ${HAL_DIR}/stm32g4xx_hal_i2c_ex.c
    ${HAL_DIR}/stm32g4xx_hal_iwdg.c
    ${HAL_DIR}/stm32g4xx_hal_spi.c
    ${HAL_DIR}/stm32g4xx_hal_spi_ex.c
    ${HAL_DIR}/stm32g4xx_hal_tim.c
    ${HAL_DIR}/stm32g4xx_hal_tim_ex.c
    ${HAL_DIR}/stm32g4xx_hal_uart.c
    ${HAL_DIR}/stm32g4xx_hal_uart_ex.c

    # 公共层
    ${CMAKE_SOURCE_DIR}/libraries/zf_common/zf_common_bsp.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_common/zf_common_clock.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_common/zf_common_debug.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_common/zf_common_delay.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_common/zf_common_fault.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_common/zf_common_fifo.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_common/zf_common_interrupt.c

    # 芯片外设驱动层
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_adc.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_can.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_encoder.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_gpio.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_i2c.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_pit.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_pwm.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_spi.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_timer.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_uart.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver/zf_driver_watchdog.c

    # 外接设备驱动层
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_buzzer.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_imu660ra.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_key.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_lcd_fonts.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_lcd_hw.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_lcd_image.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_lcd_user.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_led.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_nrf24l01.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_power.c
    ${CMAKE_SOURCE_DIR}/libraries/zf_device/zf_device_servo.c

    # 框架入口
    ${CMAKE_SOURCE_DIR}/project/user/src/main.c
    ${CMAKE_SOURCE_DIR}/project/user/src/isr.c

    # 用户业务代码
    ${CMAKE_SOURCE_DIR}/project/code/menu_core.c
    ${CMAKE_SOURCE_DIR}/project/code/menu_key.c
    ${CMAKE_SOURCE_DIR}/project/code/menu_user.c
    ${CMAKE_SOURCE_DIR}/project/code/menu_view.c
)

# ---------------------------------------------------------------------------
# 头文件搜索路径
# ---------------------------------------------------------------------------
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/libraries/sdk
    ${CMAKE_SOURCE_DIR}/libraries/zf_common
    ${CMAKE_SOURCE_DIR}/libraries/zf_driver
    ${CMAKE_SOURCE_DIR}/libraries/zf_device
    ${CMAKE_SOURCE_DIR}/project/code
)

# ---------------------------------------------------------------------------
# 宏定义
# ---------------------------------------------------------------------------
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32G474xx
    $<$<CONFIG:Debug>:DEBUG>
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    m
)

# Generate .hex/.bin next to the .elf for flashing tools
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex and .bin"
)
