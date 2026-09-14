#ifndef _RAPFI_PROTOCOL_H_
#define _RAPFI_PROTOCOL_H_

/**
 * @file rapfi_protocol.h
 * @brief Rapfi-Embedded 上位机 <-> STM32 的二进制协议（定长小端帧）。
 *
 * 设计目标：STM32 端零字符串解析（无 sscanf / strcmp / vsnprintf），
 * 每条命令定长 4 字节，应答按类型定长，并把“轮到谁/胜负”随应答一起带回，
 * 省掉单独的 STATUS 往返。
 *
 * ---- 命令（主机 -> MCU，4 字节）----
 *   [op][a][b][c]
 *   0x01 NEW                       新局，黑先行
 *   0x02 PLAY   a=x  b=y           当前方落子（0..14）
 *   0x03 GO     a=depth b=ms_lo c=ms_hi   引擎为当前方思考；depth=0 用默认
 *   0x04 UNDO                      撤销一手
 *   0x05 STATUS                    查询状态
 *   0x06 TURN                      查询轮到谁
 *   0x07 PING                      连通性
 *
 * ---- 应答（MCU -> 主机，首字节为类型）----
 *   0x80 OK     [0x80][status][turn]                         3 字节
 *   0x81 ERR    [0x81]                                       1 字节
 *   0x82 MOVE   [0x82][x][y][depth][score i32][nodes u32][ms u32][status][turn]
 *                                                            18 字节
 *   0x83 STATUS [0x83][status][turn]                         3 字节
 *   0x84 TURN   [0x84][turn]                                 2 字节
 *   0x85 READY  [0x85]                                       1 字节
 *
 *   turn:   0=黑 1=白
 *   status: 0=进行中 1=黑胜 2=白胜 3=平局
 */

/* 命令 */
#define RAPFI_CMD_NEW     0x01U
#define RAPFI_CMD_PLAY    0x02U
#define RAPFI_CMD_GO      0x03U
#define RAPFI_CMD_UNDO    0x04U
#define RAPFI_CMD_STATUS  0x05U
#define RAPFI_CMD_TURN    0x06U
#define RAPFI_CMD_PING    0x07U

/* 应答 */
#define RAPFI_RSP_OK      0x80U
#define RAPFI_RSP_ERR     0x81U
#define RAPFI_RSP_MOVE    0x82U
#define RAPFI_RSP_STATUS  0x83U
#define RAPFI_RSP_TURN    0x84U
#define RAPFI_RSP_READY   0x85U

/* 状态码 */
#define RAPFI_ST_PLAYING  0U
#define RAPFI_ST_BLACK    1U
#define RAPFI_ST_WHITE    2U
#define RAPFI_ST_DRAW     3U

/** @brief 命令帧长度。 */
#define RAPFI_CMD_LEN     4U
/** @brief MOVE 应答帧长度。 */
#define RAPFI_MOVE_LEN    18U
/** @brief OK / STATUS 应答帧长度。 */
#define RAPFI_OK_LEN      3U

#endif /* _RAPFI_PROTOCOL_H_ */
