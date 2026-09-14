#ifndef _rp_common_interrupt_h_
#define _rp_common_interrupt_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化中断优先级分组。
 * @return 无。
 * @note 将 NVIC 优先级分组设置为 NVIC_PRIORITYGROUP_4（4 位抢占，0 位子优先级），
 *       只需在系统初始化时调用一次。
 */
void interrupt_init(void);

/**
 * @brief 关闭全局中断并返回此前的 PRIMASK。
 * @return 调用前的 PRIMASK 值。
 * @note 与 interrupt_global_enable() 成对使用，支持简单嵌套。
 */
uint32_t interrupt_global_disable(void);

/**
 * @brief 按保存的 PRIMASK 恢复全局中断状态。
 * @param primask interrupt_global_disable() 的返回值。
 * @return 无。
 */
void interrupt_global_enable(uint32_t primask);

/**
 * @brief 使能指定外设中断。
 * @param irqn 中断号，可查看 isr.c 中的中断服务函数标注。
 * @return 无。
 */
void interrupt_enable(IRQn_Type irqn);

/**
 * @brief 屏蔽指定外设中断。
 * @param irqn 中断号，可查看 isr.c 中的中断服务函数标注。
 * @return 无。
 */
void interrupt_disable(IRQn_Type irqn);

/**
 * @brief 设置指定外设中断的优先级。
 * @param irqn 中断号。
 * @param preempt 抢占优先级。
 * @param sub 子优先级。
 * @return 无。
 * @note 使用 NVIC_GetPriorityGrouping() 读取当前分组后进行编码。
 */
void interrupt_set_priority(IRQn_Type irqn, uint8_t preempt, uint8_t sub);

#ifdef __cplusplus
}
#endif

#endif /* _rp_common_interrupt_h_ */
