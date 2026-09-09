#include "zf_driver_watchdog.h"

/** @brief LSI 时钟频率，单位 Hz。 */
#define IWDG_LSI_HZ 32000U

/** @brief IWDG 重装寄存器最大值，对应 reload + 1 的上限。 */
#define IWDG_RELOAD_MAX 4096U

/** @brief IWDG 分频系数数量（4~256 共 7 档）。 */
#define IWDG_PRESCALER_COUNT 7U

/** @brief IWDG 句柄。 */
static IWDG_HandleTypeDef iwdg_handle;

/** @brief IWDG 就绪标志。 */
static bool iwdg_ready;

zf_status_t iwdg_init(uint32_t timeout_ms)
{
  static const uint32_t prescaler_table[IWDG_PRESCALER_COUNT] = {
    IWDG_PRESCALER_4,   IWDG_PRESCALER_8,   IWDG_PRESCALER_16, IWDG_PRESCALER_32,
    IWDG_PRESCALER_64,  IWDG_PRESCALER_128, IWDG_PRESCALER_256,
  };
  uint32_t index;
  uint32_t divider;
  uint64_t reload_plus1;

  if (timeout_ms == 0U) {
    timeout_ms = IWDG_DEFAULT_TIMEOUT_MS;
  }
  if (timeout_ms == 0U) {
    return ZF_INVALID_PARAM;
  }

  /* 超时 = 4 * divider * (reload + 1) / 32000 秒，向上取整求 reload + 1 */
  for (index = 0U; index < IWDG_PRESCALER_COUNT; ++index) {
    divider = 4U << index;
    reload_plus1 = (((uint64_t)timeout_ms * IWDG_LSI_HZ) + (4ULL * divider * 1000ULL) - 1ULL) /
                   (4ULL * divider * 1000ULL);
    if (reload_plus1 <= IWDG_RELOAD_MAX) {
      break;
    }
  }
  if (index >= IWDG_PRESCALER_COUNT) {
    return ZF_INVALID_PARAM;
  }

  iwdg_handle.Instance = IWDG;
  iwdg_handle.Init.Prescaler = prescaler_table[index];
  iwdg_handle.Init.Reload = (uint32_t)reload_plus1 - 1U;
  iwdg_handle.Init.Window = IWDG_WINDOW_DISABLE;
  if (HAL_IWDG_Init(&iwdg_handle) != HAL_OK) {
    return ZF_ERROR;
  }

  iwdg_ready = true;
  return ZF_OK;
}

void iwdg_feed(void)
{
  if (!iwdg_ready) {
    return;
  }
  (void)HAL_IWDG_Refresh(&iwdg_handle);
}
