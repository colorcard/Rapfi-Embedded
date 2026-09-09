#include "zf_common_headfile.h"
#include "menu_core.h"

/******************************************************************************/
/*                       Cortex-M4 处理器异常处理                              */
/******************************************************************************/

void NMI_Handler(void)
{
  while (1) {
  }
}

void HardFault_Handler(void)
{
  while (1) {
  }
}

void MemManage_Handler(void)
{
  while (1) {
  }
}

void BusFault_Handler(void)
{
  while (1) {
  }
}

void UsageFault_Handler(void)
{
  while (1) {
  }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

/******************************************************************************/
/*                          外设中断服务函数                                    */
/******************************************************************************/

/**
 * @brief TIM1 触发/换相中断与 TIM17 全局中断（共用向量）。
 * @return 无。
 */
void TIM1_TRG_COM_TIM17_IRQHandler(void)
{
  TIM_HandleTypeDef *tim1 = timer_get_handle(TIMER_1);
  TIM_HandleTypeDef *tim17 = timer_get_handle(TIMER_17);

  if ((tim1 != NULL) && (tim1->Instance != NULL)) {
    HAL_TIM_IRQHandler(tim1);
  }
  if ((tim17 != NULL) && (tim17->Instance != NULL)) {
    HAL_TIM_IRQHandler(tim17);
  }
}

/**
 * @brief 定时器周期完成回调，用于 10 ms 按键扫描。
 * @param htim 触发周期完成事件的定时器句柄。
 * @return 无。
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == timer_get_handle(TIMER_17)) {
    menu_key_scan();
  }
}

/**
 * @brief USART1 全局中断，用于接收缓冲。
 * @return 无。
 */
void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(uart_get_handle(UART_1));
}

/**
 * @brief USART2 全局中断，用于接收缓冲。
 * @return 无。
 */
void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(uart_get_handle(UART_2));
}

/**
 * @brief USART3 全局中断，用于接收缓冲。
 * @return 无。
 */
void USART3_IRQHandler(void)
{
  HAL_UART_IRQHandler(uart_get_handle(UART_3));
}
