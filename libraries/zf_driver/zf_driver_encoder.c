#include "zf_driver_encoder.h"

zf_status_t encoder_init(encoder_index_enum encoder)
{
  (void)encoder;
  return ZF_NOT_READY;
}

int32_t encoder_get_count(encoder_index_enum encoder)
{
  (void)encoder;
  return 0;
}

void encoder_reset(encoder_index_enum encoder)
{
  (void)encoder;
}
