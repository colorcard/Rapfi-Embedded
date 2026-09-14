#include "rp_common_fifo.h"

#include <string.h>

/**
 * @brief 判断 FIFO 对象是否可用。
 * @param fifo FIFO 对象指针。
 * @return 1 表示可用，0 表示不可用。
 */
static uint8_t fifo_is_valid(const rp_fifo_t *fifo)
{
  if ((fifo == NULL) || (fifo->buffer == NULL) || (fifo->capacity == 0U)) {
    return 0U;
  }
  return 1U;
}

void fifo_init(rp_fifo_t *fifo, uint8_t *buffer, uint32_t capacity)
{
  if (fifo == NULL) {
    return;
  }

  if ((buffer == NULL) || (capacity == 0U)) {
    fifo->buffer = NULL;
    fifo->capacity = 0U;
  } else {
    fifo->buffer = buffer;
    fifo->capacity = capacity;
  }

  fifo->head = 0U;
  fifo->tail = 0U;
}

void fifo_clear(rp_fifo_t *fifo)
{
  if (fifo == NULL) {
    return;
  }

  fifo->head = 0U;
  fifo->tail = 0U;
}

uint32_t fifo_used(const rp_fifo_t *fifo)
{
  uint32_t head;
  uint32_t tail;

  if (fifo_is_valid(fifo) == 0U) {
    return 0U;
  }

  /* head/tail 为单调递增的逻辑指针，无符号差值在 32 位回绕后依然正确。 */
  head = fifo->head;
  tail = fifo->tail;
  return head - tail;
}

uint32_t fifo_free(const rp_fifo_t *fifo)
{
  if (fifo_is_valid(fifo) == 0U) {
    return 0U;
  }

  return fifo->capacity - fifo_used(fifo);
}

rp_status_t fifo_write_byte(rp_fifo_t *fifo, uint8_t data)
{
  if (fifo_is_valid(fifo) == 0U) {
    return RP_INVALID_PARAM;
  }

  if ((fifo->head - fifo->tail) >= fifo->capacity) {
    return RP_ERROR;
  }

  fifo->buffer[fifo->head % fifo->capacity] = data;
  fifo->head += 1U;
  return RP_OK;
}

rp_status_t fifo_read_byte(rp_fifo_t *fifo, uint8_t *data)
{
  if ((fifo_is_valid(fifo) == 0U) || (data == NULL)) {
    return RP_INVALID_PARAM;
  }

  if (fifo->head == fifo->tail) {
    return RP_ERROR;
  }

  *data = fifo->buffer[fifo->tail % fifo->capacity];
  fifo->tail += 1U;
  return RP_OK;
}

uint32_t fifo_write_buffer(rp_fifo_t *fifo, const uint8_t *data, uint32_t length)
{
  uint32_t used;
  uint32_t space;
  uint32_t count;
  uint32_t index;
  uint32_t first;

  if ((fifo_is_valid(fifo) == 0U) || (data == NULL) || (length == 0U)) {
    return 0U;
  }

  used = fifo->head - fifo->tail;
  space = fifo->capacity - used;
  count = (length < space) ? length : space;
  if (count == 0U) {
    return 0U;
  }

  index = fifo->head % fifo->capacity;
  first = fifo->capacity - index;
  if (first > count) {
    first = count;
  }

  memcpy(&fifo->buffer[index], data, first);
  if (count > first) {
    memcpy(&fifo->buffer[0], &data[first], count - first);
  }

  fifo->head += count;
  return count;
}

uint32_t fifo_read_buffer(rp_fifo_t *fifo, uint8_t *data, uint32_t length)
{
  uint32_t used;
  uint32_t count;
  uint32_t index;
  uint32_t first;

  if ((fifo_is_valid(fifo) == 0U) || (data == NULL) || (length == 0U)) {
    return 0U;
  }

  used = fifo->head - fifo->tail;
  count = (length < used) ? length : used;
  if (count == 0U) {
    return 0U;
  }

  index = fifo->tail % fifo->capacity;
  first = fifo->capacity - index;
  if (first > count) {
    first = count;
  }

  memcpy(&data[0], &fifo->buffer[index], first);
  if (count > first) {
    memcpy(&data[first], &fifo->buffer[0], count - first);
  }

  fifo->tail += count;
  return count;
}
