#ifndef _rp_common_headfile_h_
#define _rp_common_headfile_h_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ========================= 公共层 ========================= */
#include "rp_common_bsp_config.h"
#include "rp_common_bsp.h"
#include "rp_common_clock.h"
#include "rp_common_debug.h"
#include "rp_common_delay.h"
#include "rp_common_fault.h"
#include "rp_common_fifo.h"
#include "rp_common_interrupt.h"

/* ====================== 芯片外设驱动层 ====================== */
#include "rp_driver_adc.h"
#include "rp_driver_can.h"
#include "rp_driver_encoder.h"
#include "rp_driver_gpio.h"
#include "rp_driver_i2c.h"
#include "rp_driver_pit.h"
#include "rp_driver_pwm.h"
#include "rp_driver_rtc.h"
#include "rp_driver_spi.h"
#include "rp_driver_timer.h"
#include "rp_driver_uart.h"
#include "rp_driver_watchdog.h"

/* ====================== 外接设备驱动层 ====================== */
#include "rp_device_buzzer.h"
#include "rp_device_imu660ra.h"
#include "rp_device_key.h"
#include "rp_device_lcd_hw.h"
#include "rp_device_led.h"
#include "rp_device_nrf24l01.h"
#include "rp_device_power.h"
#include "rp_device_servo.h"
#include "rp_device_usb_cdc.h"
#include "rp_device_w25q128.h"

#endif /* _rp_common_headfile_h_ */
