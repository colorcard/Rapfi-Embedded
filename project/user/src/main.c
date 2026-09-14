#include "zf_common_headfile.h"
#include "zf_device_usb_cdc.h"

/** @brief 单行命令最大长度。 */
#define LINE_MAX 160U

static char s_line[LINE_MAX];
static uint32_t s_line_len;

/**
 * @brief 处理一条来自 USB CDC 的文本命令（占位实现，后续接入引擎）。
 * @param line 以 '\0' 结尾的命令行。
 * @return 无。
 */
static void protocol_handle_line(const char *line)
{
  usb_cdc_printf("ECHO %s\r\n", line);
}

/**
 * @brief 应用入口。
 * @return 不会返回。
 * @note 初始化顺序：HAL -> 时钟 -> 板级外设 -> ST7789 -> USB CDC。
 */
int main(void)
{
  uint8_t byte;

  /* 让总线错误精确上报，便于定位非法访问。 */
  (*(volatile uint32_t *)0xE000E008UL) |= (1UL << 1U);

  HAL_Init();
  clock_init();
  if (bsp_init() != ZF_OK) {
    error_handler();
  }

  debug_init();
  (void)lcd_hw_init();
  (void)usb_cdc_init();

  usb_cdc_write_str("\r\nRapfi-Embedded ready.\r\n> ");
  s_line_len = 0U;

  while (1) {
    while (usb_cdc_try_read_byte(&byte) != 0) {
      if (byte == (uint8_t)'\n') {
        s_line[s_line_len] = '\0';
        protocol_handle_line(s_line);
        s_line_len = 0U;
      } else if ((byte != (uint8_t)'\r') && (byte != 0U)) {
        if (s_line_len < (LINE_MAX - 1U)) {
          s_line[s_line_len] = (char)byte;
          ++s_line_len;
        } else {
          s_line_len = 0U; /* 行过长丢弃 */
        }
      }
    }

#if (BSP_ENABLE_IWDG != 0U)
    iwdg_feed();
#endif
  }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief 参数断言失败处理。
 * @param file 源文件名。
 * @param line 出错行号。
 * @return 无。
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
  while (1) {
  }
}
#endif
