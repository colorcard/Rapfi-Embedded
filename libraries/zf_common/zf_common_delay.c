#include "zf_common_delay.h"
#include "stm32g4xx.h"

#define DELAY_US_CHUNK_MAX 1000000UL

static uint8_t delay_initialized;

void system_delay_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  delay_initialized = 1U;
}

static void Delay_Cycles(uint32_t cycles)
{
  uint32_t start = DWT->CYCCNT;

  while ((uint32_t)(DWT->CYCCNT - start) < cycles)
  {
    __NOP();
  }
}

void system_delay_us(uint32_t us)
{
  uint32_t cycles_per_us;

  if (delay_initialized == 0U)
  {
    system_delay_init();
  }

  cycles_per_us = SystemCoreClock / 1000000UL;
  if (cycles_per_us == 0U)
  {
    cycles_per_us = 1U;
  }

  /* Chunking prevents the 32-bit cycle multiplication from overflowing. */
  while (us > DELAY_US_CHUNK_MAX)
  {
    Delay_Cycles(cycles_per_us * DELAY_US_CHUNK_MAX);
    us -= DELAY_US_CHUNK_MAX;
  }

  if (us != 0U)
  {
    Delay_Cycles(cycles_per_us * us);
  }
}

void system_delay_ms(uint32_t ms)
{
  while (ms-- != 0U)
  {
    system_delay_us(1000U);
  }
}
