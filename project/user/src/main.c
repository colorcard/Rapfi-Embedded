#include "zf_common_headfile.h"
#include "menu_core.h"

/**
 * @brief 应用入口。
 * @return 不会返回。
 * @note 初始化顺序：HAL -> 时钟 -> 板级外设 -> 显示 -> 菜单。
 */
int main(void)
{
  HAL_Init();
  clock_init();
  if (bsp_init() != ZF_OK) {
    error_handler();
  }

  lcd_init(LCD_DIRECTION_LANDSCAPE);
  menu_init(50U);

  while (1) {
    menu_process();
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
