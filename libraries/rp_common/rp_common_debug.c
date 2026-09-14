#include "rp_common_debug.h"

#include <stdarg.h>
#include <stdio.h>

#include "rp_driver_uart.h"

/** @brief 调试串口就绪标志，0 表示尚未初始化。 */
static volatile uint8_t debug_ready = 0U;

/** @brief debug_printf 使用的静态格式化缓冲区，不使用动态内存。 */
static char debug_printf_buffer[DEBUG_PRINTF_CAPACITY];

void debug_init(void)
{
  if (uart_init(DEBUG_UART_INDEX, NULL) == RP_OK) {
    debug_ready = 1U;
  }
}

void debug_write(const uint8_t *data, uint16_t length)
{
  if ((debug_ready == 0U) || (data == NULL) || (length == 0U)) {
    return;
  }

  (void)uart_write_buffer(DEBUG_UART_INDEX, data, length, DEBUG_UART_TIMEOUT_MS);
}

void debug_printf(const char *format, ...)
{
  va_list args;
  int length;

  if ((debug_ready == 0U) || (format == NULL)) {
    return;
  }

  va_start(args, format);
  length = vsnprintf(debug_printf_buffer, DEBUG_PRINTF_CAPACITY, format, args);
  va_end(args);

  if (length <= 0) {
    return;
  }

  if (length >= (int)DEBUG_PRINTF_CAPACITY) {
    length = (int)DEBUG_PRINTF_CAPACITY - 1;
  }

  (void)uart_write_buffer(DEBUG_UART_INDEX, (const uint8_t *)debug_printf_buffer,
                          (uint16_t)length, DEBUG_UART_TIMEOUT_MS);
}
