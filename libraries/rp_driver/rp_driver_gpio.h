#ifndef _rp_driver_gpio_h_
#define _rp_driver_gpio_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief GPIO 初始化参数。 */
typedef struct {
  GPIO_TypeDef *port; /**< GPIO 端口，支持 GPIOA~GPIOG。 */
  uint16_t pin;       /**< GPIO_PIN_x 或按位或。 */
  uint32_t mode;      /**< GPIO_MODE_INPUT / OUTPUT_PP / OUTPUT_OD / AF_PP / AF_OD / ANALOG / IT_xxx。 */
  uint32_t pull;      /**< GPIO_NOPULL / GPIO_PULLUP / GPIO_PULLDOWN。 */
  uint32_t speed;     /**< GPIO_SPEED_FREQ_xxx。 */
  uint32_t alternate; /**< GPIO_AFx_xxx，仅复用模式使用，其余填 0U。 */
} gpio_cfg_t;

/**
 * @brief 按配置初始化 GPIO。
 * @param cfg 配置参数。
 * @return 无。
 * @note 会根据端口自动使能对应 RCC 时钟；cfg 为空、端口为空或引脚为 0 时直接返回。
 */
void gpio_init(const gpio_cfg_t *cfg);

/**
 * @brief 设置引脚输出电平。
 * @param port GPIO 端口。
 * @param pin 引脚掩码，可为多个 GPIO_PIN_x 的按位或。
 * @param state GPIO_PIN_SET 或 GPIO_PIN_RESET。
 * @return 无。
 */
void gpio_set_level(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);

/**
 * @brief 读取引脚输入电平。
 * @param port GPIO 端口。
 * @param pin 引脚掩码，可为多个 GPIO_PIN_x 的按位或。
 * @return 引脚电平；参数非法时返回 GPIO_PIN_RESET。
 */
GPIO_PinState gpio_get_level(GPIO_TypeDef *port, uint16_t pin);

/**
 * @brief 翻转引脚输出电平。
 * @param port GPIO 端口。
 * @param pin 引脚掩码，可为多个 GPIO_PIN_x 的按位或。
 * @return 无。
 */
void gpio_toggle_level(GPIO_TypeDef *port, uint16_t pin);

#ifdef __cplusplus
}
#endif

#endif /* _rp_driver_gpio_h_ */
