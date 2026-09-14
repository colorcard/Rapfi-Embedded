#include "gomoku.h"

#include <stdio.h>
#include <string.h>

#include "stm32g4xx_hal.h"

/** @brief 每格最多所属的 5 连窗口数（4 方向 × 5 个偏移）。 */
#define GWIN_PER_CELL   20
/** @brief 5 连窗口总数上限（横 165 + 竖 165 + 斜 121×2）。 */
#define GWIN_MAX        600
/** @brief 搜索最大层数。 */
#define MAX_PLY         16
/** @brief 候选点数量上限。 */
#define MAX_CAND        128
/** @brief 根节点保留的候选数。 */
#define ROOT_CAND       16
/** @brief 内部节点保留的候选数。 */
#define NODE_CAND       12
/** @brief 胜负分。 */
#define SCORE_WIN       10000000

/** @brief 窗口内本方子数对应的分值（下标 0..5）。 */
static const int32_t s_win_score[6] = {0, 1, 12, 200, 4000, SCORE_WIN};

/* ------------------------------ 棋盘状态 ------------------------------ */

static uint8_t s_cell[GOMOKU_CELLS];
static uint8_t s_near[GOMOKU_CELLS]; /* 附近 2 格内的邻子计数，增量维护 */
static uint8_t s_hist_cell[GOMOKU_CELLS];
static uint8_t s_hist_side[GOMOKU_CELLS];
static int s_hist_n;
static uint8_t s_winner;
/** @brief 增量维护的窗口总分（按方，下标 1/2）。 */
static int32_t s_score[3];

/* ------------------------------ 窗口索引 ------------------------------ */

static uint16_t s_win[GWIN_MAX][5];
static uint16_t s_nwin;
static uint8_t s_cell_win[GOMOKU_CELLS][GWIN_PER_CELL];
static uint8_t s_cell_win_n[GOMOKU_CELLS];

/* ------------------------------ 搜索状态 ------------------------------ */

static uint16_t s_cand[MAX_PLY][MAX_CAND];
static int32_t s_cand_score[MAX_PLY][MAX_CAND];
static int s_ncand[MAX_PLY];
static uint16_t s_killer[MAX_PLY][2];
static uint32_t s_nodes;
static uint32_t s_deadline;
static int s_time_limited;
static int s_abort;

/**
 * @brief 构建 5 连窗口索引（初始化时调用一次）。
 * @return 无。
 */
void gomoku_init(void)
{
  static const int dx[4] = {1, 0, 1, 1};
  static const int dy[4] = {0, 1, 1, -1};
  int dir;
  int x;
  int y;

  s_nwin = 0U;
  memset(s_cell_win_n, 0, sizeof(s_cell_win_n));

  for (dir = 0; dir < 4; ++dir) {
    for (y = 0; y < GOMOKU_N; ++y) {
      for (x = 0; x < GOMOKU_N; ++x) {
        int k;
        int ok = 1;
        int ex = x + dx[dir] * 4;
        int ey = y + dy[dir] * 4;
        if ((ex < 0) || (ex >= GOMOKU_N) || (ey < 0) || (ey >= GOMOKU_N)) {
          ok = 0;
        }
        if ((s_nwin >= GWIN_MAX) || (ok == 0)) {
          continue;
        }
        for (k = 0; k < 5; ++k) {
          int cx = x + dx[dir] * k;
          int cy = y + dy[dir] * k;
          int idx = cy * GOMOKU_N + cx;
          uint8_t n = s_cell_win_n[idx];
          s_win[s_nwin][k] = (uint16_t)idx;
          if (n < GWIN_PER_CELL) {
            s_cell_win[idx][n] = (uint8_t)s_nwin;
            s_cell_win_n[idx] = (uint8_t)(n + 1U);
          }
        }
        ++s_nwin;
      }
    }
  }
}

void gomoku_new(void)
{
  memset(s_cell, GOMOKU_EMPTY, sizeof(s_cell));
  memset(s_near, 0, sizeof(s_near));
  s_score[0] = 0;
  s_score[1] = 0;
  s_score[2] = 0;
  s_hist_n = 0;
  s_winner = GOMOKU_EMPTY;
}

/**
 * @brief 增量更新窗口总分：假设 idx 处落下/撤销一枚 side 子。
 * @param idx 格子索引（调用时该格应为空）。
 * @param side 落子方。
 * @param sign +1 落子，-1 撤销。
 * @return 无。
 */
static void score_apply(int idx, int side, int sign)
{
  int opp = 3 - side;
  uint8_t n = s_cell_win_n[idx];
  uint8_t i;

  for (i = 0U; i < n; ++i) {
    const uint16_t *c = s_win[s_cell_win[idx][i]];
    int mine = 0;
    int theirs = 0;
    int k;
    for (k = 0; k < 5; ++k) {
      uint8_t v;
      if (c[k] == (uint16_t)idx) {
        continue;
      }
      v = s_cell[c[k]];
      if (v == (uint8_t)side) {
        ++mine;
      } else if (v == (uint8_t)opp) {
        ++theirs;
      }
    }
    {
      int32_t old_side = (theirs == 0) ? s_win_score[mine] : 0;
      int32_t old_opp = (mine == 0) ? s_win_score[theirs] : 0;
      int32_t new_side = (theirs == 0) ? s_win_score[mine + 1] : 0;
      s_score[side] += (int32_t)sign * (new_side - old_side);
      s_score[opp] += (int32_t)sign * (0 - old_opp);
    }
  }
}

/**
 * @brief 落子/撤销时增量维护“附近有子”计数。
 * @param idx 格子索引。
 * @param delta +1 落子，-1 撤销。
 * @return 无。
 */
static void near_update(int idx, int delta)
{
  int x = idx % GOMOKU_N;
  int y = idx / GOMOKU_N;
  int dy;

  for (dy = -2; dy <= 2; ++dy) {
    int ny = y + dy;
    int dx;
    if ((ny < 0) || (ny >= GOMOKU_N)) {
      continue;
    }
    for (dx = -2; dx <= 2; ++dx) {
      int nx = x + dx;
      int j;
      if ((nx < 0) || (nx >= GOMOKU_N) || ((dx == 0) && (dy == 0))) {
        continue;
      }
      j = ny * GOMOKU_N + nx;
      if (delta > 0) {
        if (s_near[j] < 255U) {
          ++s_near[j];
        }
      } else if (s_near[j] > 0U) {
        --s_near[j];
      }
    }
  }
}

/**
 * @brief 落子（内部）：更新增量分数与邻域计数并写盘。
 * @param idx 格子索引（应为空）。
 * @param side 落子方。
 * @return 无。
 */
static void make_move(int idx, int side)
{
  score_apply(idx, side, 1);
  s_cell[idx] = (uint8_t)side;
  near_update(idx, 1);
}

/**
 * @brief 撤销（内部）：与 make_move 对称。
 * @param idx 格子索引。
 * @param side 落子方。
 * @return 无。
 */
static void unmake_move(int idx, int side)
{
  s_cell[idx] = GOMOKU_EMPTY;
  score_apply(idx, side, -1);
  near_update(idx, -1);
}

int gomoku_side_at(int x, int y)
{
  if ((x < 0) || (x >= GOMOKU_N) || (y < 0) || (y >= GOMOKU_N)) {
    return -1;
  }
  return (int)s_cell[y * GOMOKU_N + x];
}

int gomoku_stone_count(void)
{
  return s_hist_n;
}

int gomoku_last_move(int *x, int *y)
{
  if (s_hist_n == 0) {
    return -1;
  }
  if (x != NULL) {
    *x = (int)(s_hist_cell[s_hist_n - 1] % GOMOKU_N);
  }
  if (y != NULL) {
    *y = (int)(s_hist_cell[s_hist_n - 1] / GOMOKU_N);
  }
  return 0;
}

/**
 * @brief 判断在 idx 落 side 后是否形成五连。
 * @param idx 格子索引。
 * @param side 落子方。
 * @return 1 成五，0 否。
 */
static int makes_five(int idx, int side)
{
  static const int dx[4] = {1, 0, 1, 1};
  static const int dy[4] = {0, 1, 1, -1};
  int x = idx % GOMOKU_N;
  int y = idx / GOMOKU_N;
  int dir;

  for (dir = 0; dir < 4; ++dir) {
    int count = 1;
    int sgn;
    for (sgn = -1; sgn <= 1; sgn += 2) {
      int cx = x + dx[dir] * sgn;
      int cy = y + dy[dir] * sgn;
      while ((cx >= 0) && (cx < GOMOKU_N) && (cy >= 0) && (cy < GOMOKU_N) &&
             (s_cell[cy * GOMOKU_N + cx] == (uint8_t)side)) {
        ++count;
        cx += dx[dir] * sgn;
        cy += dy[dir] * sgn;
      }
    }
    if (count >= 5) {
      return 1;
    }
  }
  return 0;
}

int gomoku_place(int x, int y, int side)
{
  int idx;

  if ((x < 0) || (x >= GOMOKU_N) || (y < 0) || (y >= GOMOKU_N)) {
    return -1;
  }
  if ((side != GOMOKU_BLACK) && (side != GOMOKU_WHITE)) {
    return -1;
  }
  idx = y * GOMOKU_N + x;
  if (s_cell[idx] != GOMOKU_EMPTY) {
    return -1;
  }
  make_move(idx, side);
  s_hist_cell[s_hist_n] = (uint8_t)idx;
  s_hist_side[s_hist_n] = (uint8_t)side;
  ++s_hist_n;
  if (makes_five(idx, side) != 0) {
    s_winner = (uint8_t)side;
  }
  return 0;
}

int gomoku_undo(void)
{
  int idx;
  int side;

  if (s_hist_n == 0) {
    return -1;
  }
  --s_hist_n;
  idx = s_hist_cell[s_hist_n];
  side = s_hist_side[s_hist_n];
  unmake_move(idx, side);
  if (s_winner == (uint8_t)side) {
    s_winner = GOMOKU_EMPTY;
  }
  return 0;
}

int gomoku_status(void)
{
  if (s_winner != GOMOKU_EMPTY) {
    return (int)s_winner;
  }
  if (s_hist_n >= GOMOKU_CELLS) {
    return GOMOKU_DRAW;
  }
  return GOMOKU_EMPTY;
}

/* ------------------------------ 评估 ------------------------------ */

/**
 * @brief 整盘窗口评估：统计所有不含对方子的 5 连窗口。
 * @param side 评估方。
 * @return 分值。
 */
static int32_t eval_side(int side)
{
  return s_score[side];
}

/**
 * @brief 落点启发：一次遍历同时算出“进攻”和“防守”价值。
 * @param idx 候选点。
 * @param side 待走方。
 * @param off 输出进攻分（自己下这里的价值）。
 * @param def 输出防守分（对手下这里的价值）。
 * @return 无。
 */
static void move_heuristic(int idx, int side, int32_t *off, int32_t *def)
{
  int opp = 3 - side;
  int32_t o = 0;
  int32_t d = 0;
  uint8_t n = s_cell_win_n[idx];
  uint8_t i;

  for (i = 0U; i < n; ++i) {
    const uint16_t *c = s_win[s_cell_win[idx][i]];
    int mine = 0;
    int theirs = 0;
    int k;
    for (k = 0; k < 5; ++k) {
      uint8_t v;
      if (c[k] == (uint16_t)idx) {
        continue;
      }
      v = s_cell[c[k]];
      if (v == (uint8_t)side) {
        ++mine;
      } else if (v == (uint8_t)opp) {
        ++theirs;
      }
    }
    if (theirs == 0) {
      o += s_win_score[mine + 1];
    }
    if (mine == 0) {
      d += s_win_score[theirs + 1];
    }
  }
  *off = o;
  *def = d;
}

/* ------------------------------ 候选生成 ------------------------------ */

/**
 * @brief 生成本层候选点（已有邻子的空点），并按启发式分值降序排序。
 * @param ply 层号。
 * @param side 待走方。
 * @return 无。
 */
static void gen_candidates(int ply, int side)
{
  int n = 0;
  int idx;

  for (idx = 0; idx < GOMOKU_CELLS; ++idx) {
    int32_t off;
    int32_t def;
    if ((s_cell[idx] != GOMOKU_EMPTY) || (s_near[idx] == 0U)) {
      continue;
    }
    if (n >= MAX_CAND) {
      break;
    }
    move_heuristic(idx, side, &off, &def);
    s_cand[ply][n] = (uint16_t)idx;
    s_cand_score[ply][n] = off * 2 + def;
    ++n;
  }

  if (n == 0) {
    int c = (GOMOKU_N / 2) * GOMOKU_N + (GOMOKU_N / 2);
    s_cand[ply][0] = (uint16_t)c;
    s_cand_score[ply][0] = 0;
    n = 1;
  }

  /* 插入排序，降序 */
  {
    int i;
    for (i = 1; i < n; ++i) {
      uint16_t ci = s_cand[ply][i];
      int32_t si = s_cand_score[ply][i];
      int j = i - 1;
      while ((j >= 0) && (s_cand_score[ply][j] < si)) {
        s_cand[ply][j + 1] = s_cand[ply][j];
        s_cand_score[ply][j + 1] = s_cand_score[ply][j];
        --j;
      }
      s_cand[ply][j + 1] = ci;
      s_cand_score[ply][j + 1] = si;
    }
  }
  s_ncand[ply] = n;
}

/* ------------------------------ 搜索 ------------------------------ */

static int negamax(int side, int depth, int alpha, int beta, int ply)
{
  int opp = 3 - side;
  int limit;
  int i;
  int best = -SCORE_WIN;

  ++s_nodes;
  if ((s_time_limited != 0) && ((s_nodes & 0x3FFU) == 0U) &&
      ((int32_t)(HAL_GetTick() - s_deadline) >= 0)) {
    s_abort = 1;
    return 0;
  }
  if (depth <= 0) {
    return (int)(eval_side(side) - eval_side(opp));
  }

  gen_candidates(ply, side);
  if (s_ncand[ply] == 0) {
    return 0;
  }
  limit = s_ncand[ply];
  if (limit > NODE_CAND) {
    limit = NODE_CAND;
  }

  for (i = 0; i < limit; ++i) {
    int idx = s_cand[ply][i];
    int score;

    make_move(idx, side);
    if (makes_five(idx, side) != 0) {
      unmake_move(idx, side);
      return SCORE_WIN - ply;
    }
    score = -negamax(opp, depth - 1, -beta, -alpha, ply + 1);
    unmake_move(idx, side);

    if (s_abort != 0) {
      return 0;
    }
    if (score > best) {
      best = score;
    }
    if (score > alpha) {
      alpha = score;
    }
    if (alpha >= beta) {
      if (s_killer[ply][0] != idx) {
        s_killer[ply][1] = s_killer[ply][0];
        s_killer[ply][0] = (uint16_t)idx;
      }
      break;
    }
  }
  return best;
}

/**
 * @brief 根节点搜索：返回最佳着法与评分。
 * @param side 待走方。
 * @param depth 深度。
 * @param best_idx 输出最佳格子索引。
 * @param out_score 输出评分。
 * @return 无。
 */
static void search_root(int side, int depth, int *best_idx, int *out_score)
{
  int opp = 3 - side;
  int alpha = -SCORE_WIN;
  int best = -SCORE_WIN;
  int best_move = -1;
  int limit;
  int i;

  s_abort = 0;
  gen_candidates(0, side);
  limit = s_ncand[0];
  if (limit > ROOT_CAND) {
    limit = ROOT_CAND;
  }

  for (i = 0; i < limit; ++i) {
    int idx = s_cand[0][i];
    int score;

    make_move(idx, side);
    if (makes_five(idx, side) != 0) {
      unmake_move(idx, side);
      *best_idx = idx;
      *out_score = SCORE_WIN - 1;
      return;
    }
    score = -negamax(opp, depth - 1, -SCORE_WIN, -alpha, 1);
    unmake_move(idx, side);

    if (s_abort != 0) {
      break;
    }
    if (score > best) {
      best = score;
      best_move = idx;
    }
    if (score > alpha) {
      alpha = score;
    }
  }

  if (best_move < 0) {
    best_move = s_cand[0][0];
    best = 0;
  }
  *best_idx = best_move;
  *out_score = best;
}

int gomoku_search(int side, int max_depth, uint32_t time_limit_ms,
                  gomoku_result_t *res)
{
  int depth;
  int best_idx = -1;
  int best_score = 0;
  uint32_t t0 = HAL_GetTick();
  int reached = 0;

  if ((side != GOMOKU_BLACK) && (side != GOMOKU_WHITE)) {
    return -1;
  }
  if (max_depth < 1) {
    max_depth = 1;
  }
  if (max_depth > MAX_PLY) {
    max_depth = MAX_PLY;
  }

  s_nodes = 0U;
  s_time_limited = (time_limit_ms > 0U) ? 1 : 0;
  s_deadline = t0 + time_limit_ms;
  s_abort = 0;
  memset(s_killer, 0xFF, sizeof(s_killer));

  for (depth = 1; depth <= max_depth; ++depth) {
    int idx = -1;
    int score = 0;
    search_root(side, depth, &idx, &score);
    if (s_abort != 0) {
      break;
    }
    best_idx = idx;
    best_score = score;
    reached = depth;
    if ((score >= SCORE_WIN - MAX_PLY) || (score <= -SCORE_WIN + MAX_PLY)) {
      break; /* 已见必胜/必败 */
    }
    if (s_ncand[0] <= 1) {
      break;
    }
  }

  if (best_idx < 0) {
    /* 兜底：任意空点 */
    int i;
    for (i = 0; i < GOMOKU_CELLS; ++i) {
      if (s_cell[i] == GOMOKU_EMPTY) {
        best_idx = i;
        break;
      }
    }
  }
  if (best_idx < 0) {
    return -1;
  }

  if (res != NULL) {
    res->x = best_idx % GOMOKU_N;
    res->y = best_idx / GOMOKU_N;
    res->score = best_score;
    res->depth = reached;
    res->nodes = s_nodes;
    res->time_ms = HAL_GetTick() - t0;
  }
  return 0;
}

int gomoku_think(int side, int max_depth, uint32_t time_limit_ms,
                 gomoku_result_t *res)
{
  gomoku_result_t r;

  if (gomoku_search(side, max_depth, time_limit_ms, &r) != 0) {
    return -1;
  }
  if (gomoku_place(r.x, r.y, side) != 0) {
    return -1;
  }
  if (res != NULL) {
    *res = r;
  }
  return 0;
}

int gomoku_to_text(char *buf, int cap)
{
  int pos = 0;
  int y;

  if ((buf == NULL) || (cap < 64)) {
    return 0;
  }
  for (y = GOMOKU_N - 1; y >= 0; --y) {
    int x;
    pos += snprintf(&buf[pos], (size_t)(cap - pos), "%2d ", y + 1);
    for (x = 0; x < GOMOKU_N; ++x) {
      uint8_t v = s_cell[y * GOMOKU_N + x];
      char ch = '.';
      if (v == GOMOKU_BLACK) {
        ch = 'X';
      } else if (v == GOMOKU_WHITE) {
        ch = 'O';
      }
      if (pos < (cap - 1)) {
        buf[pos++] = ch;
        buf[pos++] = ' ';
      }
    }
    if (pos < (cap - 1)) {
      buf[pos++] = '\r';
      buf[pos++] = '\n';
    }
  }
  pos += snprintf(&buf[pos], (size_t)(cap - pos),
                  "   A B C D E F G H I J K L M N O\r\n");
  return pos;
}
