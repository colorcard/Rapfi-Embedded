#ifndef _GOMOKU_H_
#define _GOMOKU_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 棋盘边长（15 路）。 */
#define GOMOKU_N          15
/** @brief 格子总数。 */
#define GOMOKU_CELLS      (GOMOKU_N * GOMOKU_N)

/** @brief 格子状态。 */
#define GOMOKU_EMPTY      0
#define GOMOKU_BLACK      1
#define GOMOKU_WHITE      2
/** @brief 平局/无胜者时的 winner 返回值。 */
#define GOMOKU_DRAW       3

/** @brief 搜索返回值。 */
typedef struct {
  int x;                 /**< 最佳落点列 0..14，-1 表示无可行点 */
  int y;                 /**< 最佳落点行 0..14 */
  int score;             /**< 搜索评分（正=当前方有利） */
  int depth;             /**< 实际完成深度 */
  uint32_t nodes;        /**< 搜索节点数 */
  uint32_t time_ms;      /**< 用时毫秒 */
} gomoku_result_t;

/**
 * @brief 初始化引擎（构建窗口索引，只需调用一次）。
 * @return 无。
 */
void gomoku_init(void);

/**
 * @brief 清空棋盘，回到空局。
 * @return 无。
 */
void gomoku_new(void);

/**
 * @brief 读取某点状态。
 * @param x 列 0..14。
 * @param y 行 0..14。
 * @return GOMOKU_EMPTY/BLACK/WHITE；越界返回 -1。
 */
int gomoku_side_at(int x, int y);

/**
 * @brief 落子。
 * @param x 列 0..14。
 * @param y 行 0..14。
 * @param side GOMOKU_BLACK/WHITE。
 * @return 0 成功，-1 非法。
 */
int gomoku_place(int x, int y, int side);

/**
 * @brief 撤销最近一手。
 * @return 0 成功，-1 无可撤销。
 */
int gomoku_undo(void);

/**
 * @brief 当前棋盘是否有胜者或平局。
 * @return GOMOKU_EMPTY 未结束；BLACK/WHITE 胜者；GOMOKU_DRAW 平局。
 */
int gomoku_status(void);

/**
 * @brief 已落子数。
 * @return 手数。
 */
int gomoku_stone_count(void);

/**
 * @brief 最近一手坐标。
 * @param x 输出列。
 * @param y 输出行。
 * @return 0 成功，-1 无历史。
 */
int gomoku_last_move(int *x, int *y);

/**
 * @brief 搜索当前局面的最佳着法（不自动落子）。
 * @param side 轮到的一方。
 * @param max_depth 最大深度。
 * @param time_limit_ms 时间上限（毫秒，0 表示不限）。
 * @param res 结果输出。
 * @return 0 成功，-1 无可行着法。
 */
int gomoku_search(int side, int max_depth, uint32_t time_limit_ms,
                  gomoku_result_t *res);

/**
 * @brief 搜索并落子。
 * @param side 轮到的一方。
 * @param max_depth 最大深度。
 * @param time_limit_ms 时间上限（毫秒，0 表示不限）。
 * @param res 结果输出（可为 NULL）。
 * @return 0 成功并已落子，-1 失败。
 */
int gomoku_think(int side, int max_depth, uint32_t time_limit_ms,
                 gomoku_result_t *res);

/**
 * @brief 序列化为文本棋盘（供 USB 输出）。
 * @param buf 输出缓冲。
 * @param cap 缓冲大小。
 * @return 写入的字符数（不含结尾 '\0'）。
 */
int gomoku_to_text(char *buf, int cap);

#ifdef __cplusplus
}
#endif

#endif /* _GOMOKU_H_ */
