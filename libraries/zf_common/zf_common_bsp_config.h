#ifndef _zf_common_bsp_config_h_
#define _zf_common_bsp_config_h_

#include "stm32g4xx_hal.h"

/**
 * @file bsp_config.h
 * @brief BSP 层集中默认参数。
 *
 * 原理图无法确定的数值全部集中在本文件，并带有 TODO 标注。
 * 应用层可在调用 BSP_xxx_Init() 时传入自定义配置覆盖这些默认值。
 */

/* ------------------------------- LED ---------------------------------- */
#define LED_ACTIVE_HIGH         1U /* TODO: 确认 LED 有效电平。 */

/* ------------------------------- 按键 --------------------------------- */
#define KEY_ACTIVE_LOW          1U /* TODO: 确认按下电平；当前沿用菜单逻辑（低有效）。 */

/* ------------------------------ 蜂鸣器 -------------------------------- */
#define BUZZER_ACTIVE_HIGH      1U /* TODO: 确认晶体管驱动极性。 */

/* ------------------------------- UART --------------------------------- */
#define UART_DEFAULT_BAUDRATE    115200U /* TODO: 按应用确认波特率。 */
#define UART_RX_BUFFER_SIZE      128U    /* 每路 UART 接收环形缓冲大小。 */
#define UART_IRQ_PREEMPT_PRIORITY 1U     /* TODO: 按整机实时性统一规划。 */
#define UART_IRQ_SUB_PRIORITY    0U      /* TODO: 按整机实时性统一规划。 */

/* ------------------------------- I2C ---------------------------------- */
#define I2C_DEFAULT_SPEED_HZ    400000U /* TODO: 按器件确认 100k/400k。 */
#define I2C_RISE_TIME_NS        100U    /* TODO: 按上拉/总线电容确认。 */
#define I2C_FALL_TIME_NS        10U     /* TODO: 按上拉/总线电容确认。 */

/* ------------------------------- SPI ---------------------------------- */
#define SPI_DEFAULT_PRESCALER   SPI_BAUDRATEPRESCALER_8 /* TODO: 按器件确认。 */
#define SPI_DEFAULT_CPOL        SPI_POLARITY_LOW        /* TODO: 按器件确认。 */
#define SPI_DEFAULT_CPHA        SPI_PHASE_1EDGE         /* TODO: 按器件确认。 */

/* ------------------------------- CAN ---------------------------------- */
#define CAN_DEFAULT_BITRATE     500000U /* TODO: 按总线确认。 */
#define CAN_DEFAULT_SAMPLE_PERMILLE 800U /* TODO: 按总线确认采样点。 */

/* ------------------------------- ADC ---------------------------------- */
#define ADC_DEFAULT_RESOLUTION  ADC_RESOLUTION_12B        /* TODO: 按应用确认。 */
#define ADC_DEFAULT_SAMPLING    ADC_SAMPLETIME_47CYCLES_5 /* TODO: 按源阻抗确认。 */
#define ADC_VREF_MV             3300U /* TODO: 确认 VDDA 实际电压。 */

/* ------------------------------- PWM ---------------------------------- */
#define PWM_DEFAULT_FREQUENCY_HZ 1000U /* TODO: 按电机/舵机确认，舵机常用 50 Hz。 */

/* ------------------------------- PIT ---------------------------------- */
#define PIT_DEFAULT_PERIOD_MS    10U /* 按键扫描周期，单位 ms。 */
#define PIT_IRQ_PREEMPT_PRIORITY 0U  /* TODO: 按整机实时性统一规划。 */
#define PIT_IRQ_SUB_PRIORITY     0U  /* TODO: 按整机实时性统一规划。 */

#endif /* _zf_common_bsp_config_h_ */
