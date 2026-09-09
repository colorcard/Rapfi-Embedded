#ifndef _zf_common_delay_h_
#define _zf_common_delay_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the Cortex-M4 DWT cycle counter used by the delay routines. */
void system_delay_init(void);

/* Blocking delays. Accuracy follows the current SystemCoreClock value. */
void system_delay_us(uint32_t us);
void system_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* _zf_common_delay_h_ */
