/**
 * @file lv_conf.h
 * @brief LVGL v8.3 工程配置（精简版）。
 *
 * 仅覆盖本项目用到的选项，其余由 lv_conf_internal.h 提供默认值。
 * 通过编译宏 LV_CONF_INCLUDE_SIMPLE 让 LVGL 从本目录包含该文件。
 */
#if 1 /* 置 0 可禁用本配置文件内容。 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ------------------------------- 颜色 -------------------------------- */
#define LV_COLOR_DEPTH      16  /* RGB565，与 ST7789 一致。 */
#define LV_COLOR_16_SWAP    0   /* 由 lcd_hw 负责高字节先发的字节序。 */

/* ------------------------------- 内存 -------------------------------- */
#define LV_MEM_CUSTOM       0   /* 使用 LVGL 自带静态内存池。 */
#define LV_MEM_SIZE         (24U * 1024U)
#define LV_MEM_ADR          0

/* -------------------------------- HAL -------------------------------- */
#define LV_DISP_DEF_REFR_PERIOD  30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM      0   /* 由应用周期调用 lv_tick_inc()。 */
#define LV_DPI_DEF          130

/* ------------------------------ 功能裁剪 ----------------------------- */
#define LV_USE_FILESYSTEM   0
#define LV_USE_LOG          0
#define LV_USE_ASSERT_NULL  0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

/* -------------------------------- 字体 ------------------------------- */
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/* ------------------------------- 控件 -------------------------------- */
#define LV_USE_LABEL        1
#define LV_USE_BAR          1
#define LV_USE_ARC          1
#define LV_USE_BTN          1

#endif /* LV_CONF_H */

#endif /* 配置文件内容结束 */
