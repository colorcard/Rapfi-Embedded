#include "rp_common_interrupt.h"

void interrupt_init(void)
{
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

uint32_t interrupt_global_disable(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  return primask;
}

void interrupt_global_enable(uint32_t primask)
{
  __set_PRIMASK(primask);
}

void interrupt_enable(IRQn_Type irqn)
{
  NVIC_EnableIRQ(irqn);
}

void interrupt_disable(IRQn_Type irqn)
{
  NVIC_DisableIRQ(irqn);
}

void interrupt_set_priority(IRQn_Type irqn, uint8_t preempt, uint8_t sub)
{
  NVIC_SetPriority(irqn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),
                                             preempt, sub));
}
