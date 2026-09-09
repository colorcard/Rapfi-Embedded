#ifndef _zf_common_fault_h_
#define _zf_common_fault_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 故障现场信息。
 * @note 前 8 个字段由 Cortex-M4 异常自动压栈（标准栈帧），
 *       其余字段在 fault_capture() 中从 SCB 与内核寄存器读取。
 */
typedef struct {
  uint32_t r0, r1, r2, r3, r12; /* 异常压栈的通用寄存器 */
  uint32_t lr;                  /* 压栈的 LR */
  uint32_t pc;                  /* 出错指令地址 */
  uint32_t xpsr;                /* 压栈的 xPSR */
  uint32_t exc_return;          /* 异常返回码 */
  uint32_t cfsr, hfsr, bfar, mmfar; /* SCB 故障状态寄存器 */
  uint32_t msp, psp;            /* 现场 MSP/PSP */
} fault_info_t;

/**
 * @brief 捕获故障现场。
 * @param stack_frame 异常压栈的栈帧首地址，依次为 r0,r1,r2,r3,r12,lr,pc,xpsr。
 * @param exc_return 进入异常时的 LR（EXC_RETURN）。
 * @return 无。
 * @note 结果保存到文件内静态 fault_info_t，供 fault_get_info() 读取；
 *       在 HardFault/MemManage/BusFault/UsageFault 处理函数中调用。
 */
void fault_capture(uint32_t *stack_frame, uint32_t exc_return);

/**
 * @brief 获取最近一次捕获的故障现场。
 * @return 指向静态故障现场的常量指针，不会为 NULL。
 * @note 未发生故障时各字段为 0。
 */
const fault_info_t *fault_get_info(void);

/**
 * @brief 故障处理钩子（弱函数，默认空实现）。
 * @return 无。
 * @note 应用层可定义同名强函数覆盖，用于打印 fault_get_info() 的内容；
 *       本函数返回后 fault_handler_c 会停在 while (1) 中等待复位。
 */
void fault_hook(void);

#ifdef __cplusplus
}
#endif

#endif /* _zf_common_fault_h_ */
