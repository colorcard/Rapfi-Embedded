#ifndef _rp_common_fifo_h_
#define _rp_common_fifo_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 单生产者/单消费者环形缓冲区对象。
 * @note head 只由生产者（写入方）修改，tail 只由消费者（读取方）修改；
 *       两者是单调递增的逻辑指针，物理下标由对 capacity 取模得到，
 *       因此 capacity 不要求是 2 的幂。
 *       通过无符号差值判断数据量，可完整使用 capacity 个字节。
 */
typedef struct {
  uint8_t *buffer;        /**< 用户提供的存储区，不使用动态内存。 */
  uint32_t capacity;      /**< 存储区字节数，为 0 时所有操作安全返回。 */
  volatile uint32_t head; /**< 写指针，仅生产者修改。 */
  volatile uint32_t tail; /**< 读指针，仅消费者修改。 */
} rp_fifo_t;

/**
 * @brief 初始化 FIFO。
 * @param fifo FIFO 对象指针。
 * @param buffer 用户提供的存储区指针。
 * @param capacity 存储区字节数。
 * @return 无。
 * @note buffer 为 NULL 或 capacity 为 0 时，FIFO 被置为无效状态，
 *       后续读写操作会安全返回 RP_INVALID_PARAM 或 0。
 */
void fifo_init(rp_fifo_t *fifo, uint8_t *buffer, uint32_t capacity);

/**
 * @brief 清空 FIFO 中未读取的数据。
 * @param fifo FIFO 对象指针。
 * @return 无。
 * @note 会同时复位 head 与 tail，只能在读写双方均空闲时调用，
 *       不得与中断中的读写操作并发执行。
 */
void fifo_clear(rp_fifo_t *fifo);

/**
 * @brief 查询 FIFO 中已缓存的数据字节数。
 * @param fifo FIFO 对象指针。
 * @return 已缓存字节数；对象无效时返回 0。
 */
uint32_t fifo_used(const rp_fifo_t *fifo);

/**
 * @brief 查询 FIFO 中还可写入的字节数。
 * @param fifo FIFO 对象指针。
 * @return 剩余可写字节数；对象无效时返回 0。
 */
uint32_t fifo_free(const rp_fifo_t *fifo);

/**
 * @brief 向 FIFO 写入一个字节。
 * @param fifo FIFO 对象指针。
 * @param data 待写入的字节。
 * @return RP_OK 表示成功；RP_INVALID_PARAM 表示对象无效；
 *         RP_ERROR 表示缓冲区已满。
 */
rp_status_t fifo_write_byte(rp_fifo_t *fifo, uint8_t data);

/**
 * @brief 从 FIFO 读取一个字节。
 * @param fifo FIFO 对象指针。
 * @param data 读取结果输出指针。
 * @return RP_OK 表示成功；RP_INVALID_PARAM 表示对象或输出指针无效；
 *         RP_ERROR 表示缓冲区为空。
 */
rp_status_t fifo_read_byte(rp_fifo_t *fifo, uint8_t *data);

/**
 * @brief 向 FIFO 批量写入数据。
 * @param fifo FIFO 对象指针。
 * @param data 数据来源缓冲区指针。
 * @param length 期望写入的字节数。
 * @return 实际写入的字节数；参数无效时返回 0，空间不足时写入尽可能多的数据。
 */
uint32_t fifo_write_buffer(rp_fifo_t *fifo, const uint8_t *data, uint32_t length);

/**
 * @brief 从 FIFO 批量读取数据。
 * @param fifo FIFO 对象指针。
 * @param data 读取结果输出缓冲区指针。
 * @param length 期望读取的字节数。
 * @return 实际读取的字节数；参数无效时返回 0，数据不足时读取全部可用数据。
 */
uint32_t fifo_read_buffer(rp_fifo_t *fifo, uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* _rp_common_fifo_h_ */
