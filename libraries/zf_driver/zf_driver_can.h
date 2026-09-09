#ifndef _zf_driver_can_h_
#define _zf_driver_can_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 板级 CAN 逻辑编号（当前只有 FDCAN2）。 */
typedef enum {
  CAN_2 = 0, /**< FDCAN2：PB12 RX / PB13 TX，外接 TJA1044。 */
  CAN_NUM
} can_index_enum;

/** @brief CAN 初始化参数。 */
typedef struct {
  uint32_t frame_format;          /**< FDCAN_FRAME_CLASSIC / FDCAN_FRAME_FD_BRS。 */
  uint32_t nominal_bitrate;       /**< 仲裁段速率，单位 bps。 */
  uint32_t data_bitrate;          /**< 数据段速率，单位 bps，经典 CAN 忽略。 */
  uint32_t sample_point_permille; /**< 采样点千分比，如 800 表示 80%。 */
} can_cfg_t;

/** @brief 一条 CAN 报文。 */
typedef struct {
  uint32_t id;       /**< 报文 ID。 */
  uint8_t data[64];  /**< 数据区，经典 CAN 最多 8 字节。 */
  uint8_t length;    /**< 数据字节数。 */
  bool extended;     /**< true 表示 29 位扩展 ID。 */
  bool fd_format;    /**< true 表示 CAN FD 帧。 */
} can_message_t;

/**
 * @brief 初始化 FDCAN2：重算位时序、配置验收滤波器并启动。
 * @param bus CAN 逻辑编号。
 * @param cfg 配置参数；传 NULL 使用 bsp_config.h 默认值（经典 CAN 500 kbps）。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note 本函数会配置全通滤波器并启动控制器；重复调用会重新初始化。
 */
zf_status_t can_init(can_index_enum bus, const can_cfg_t *cfg);

/**
 * @brief 启动 CAN 控制器。
 * @param bus CAN 逻辑编号。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t can_start(can_index_enum bus);

/**
 * @brief 停止 CAN 控制器。
 * @param bus CAN 逻辑编号。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t can_stop(can_index_enum bus);

/**
 * @brief 阻塞发送一条 CAN 报文。
 * @param bus CAN 逻辑编号。
 * @param message 待发送报文。
 * @param timeout_ms 等待发送完成的超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t can_send(can_index_enum bus, const can_message_t *message,
                          uint32_t timeout_ms);

/**
 * @brief 从 RX FIFO0 读取一条报文。
 * @param bus CAN 逻辑编号。
 * @param message 接收报文输出。
 * @param timeout_ms 等待报文的超时时间，单位毫秒。
 * @return ZF_OK 表示成功，ZF_TIMEOUT 表示超时。
 */
zf_status_t can_receive(can_index_enum bus, can_message_t *message,
                             uint32_t timeout_ms);

/**
 * @brief 查询 RX FIFO0 是否有待处理报文。
 * @param bus CAN 逻辑编号。
 * @return true 表示有报文。
 */
bool can_is_message_pending(can_index_enum bus);

#ifdef __cplusplus
}
#endif

#endif /* _zf_driver_can_h_ */
