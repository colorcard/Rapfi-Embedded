#include "zf_common_fault.h"

/** @brief 最近一次故障现场，未发生故障时各字段为 0。 */
static fault_info_t fault_info;

void fault_capture(uint32_t *stack_frame, uint32_t exc_return)
{
  if (stack_frame == NULL) {
    return;
  }

  fault_info.r0 = stack_frame[0];
  fault_info.r1 = stack_frame[1];
  fault_info.r2 = stack_frame[2];
  fault_info.r3 = stack_frame[3];
  fault_info.r12 = stack_frame[4];
  fault_info.lr = stack_frame[5];
  fault_info.pc = stack_frame[6];
  fault_info.xpsr = stack_frame[7];
  fault_info.exc_return = exc_return;

  fault_info.cfsr = SCB->CFSR;
  fault_info.hfsr = SCB->HFSR;
  fault_info.bfar = SCB->BFAR;
  fault_info.mmfar = SCB->MMFAR;

  fault_info.msp = __get_MSP();
  fault_info.psp = __get_PSP();
}

const fault_info_t *fault_get_info(void)
{
  return &fault_info;
}

__attribute__((weak)) void fault_hook(void)
{
}
