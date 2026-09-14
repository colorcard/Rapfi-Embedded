#include "rapfi_app.h"

#include "gomoku.h"
#include "rapfi_protocol.h"
#include "rp_device_usb_cdc.h"

/** @brief 帧装配缓冲。 */
static uint8_t s_frame[RAPFI_CMD_LEN];
static uint32_t s_frame_len;
/** @brief 上一个帧字节到达时刻，用于空闲复位。 */
static uint32_t s_frame_tick;
/** @brief 半帧超时（ms）：超过则丢弃，避免出错位后卡死。 */
#define FRAME_IDLE_TIMEOUT_MS 50U
/** @brief 当前轮到的一方。 */
static int s_turn;

/**
 * @brief 把对局胜负映射为协议状态码。
 * @return RAPFI_ST_*。
 */
static uint8_t status_code(void)
{
  int st = gomoku_status();
  if (st == GOMOKU_EMPTY) {
    return RAPFI_ST_PLAYING;
  }
  if (st == GOMOKU_DRAW) {
    return RAPFI_ST_DRAW;
  }
  return (st == GOMOKU_BLACK) ? RAPFI_ST_BLACK : RAPFI_ST_WHITE;
}

/** @brief 当前轮到方的协议编码（0=黑 1=白）。 */
static uint8_t turn_code(void)
{
  return (s_turn == GOMOKU_BLACK) ? 0U : 1U;
}

/** @brief 小端写入 u32。 */
static void put_u32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static void reply_ok(void)
{
  uint8_t b[RAPFI_OK_LEN] = {RAPFI_RSP_OK, status_code(), turn_code()};
  (void)usb_cdc_write(b, sizeof(b));
}

static void reply_err(void)
{
  uint8_t b = RAPFI_RSP_ERR;
  (void)usb_cdc_write(&b, 1U);
}

static void reply_status(void)
{
  uint8_t b[RAPFI_OK_LEN] = {RAPFI_RSP_STATUS, status_code(), turn_code()};
  (void)usb_cdc_write(b, sizeof(b));
}

static void reply_turn(void)
{
  uint8_t b[2] = {RAPFI_RSP_TURN, turn_code()};
  (void)usb_cdc_write(b, sizeof(b));
}

static void reply_move(const gomoku_result_t *r)
{
  uint8_t b[RAPFI_MOVE_LEN];
  b[0] = RAPFI_RSP_MOVE;
  b[1] = (uint8_t)r->x;
  b[2] = (uint8_t)r->y;
  b[3] = (uint8_t)r->depth;
  put_u32(&b[4], (uint32_t)r->score);
  put_u32(&b[8], r->nodes);
  put_u32(&b[12], r->time_ms);
  b[16] = status_code();
  b[17] = turn_code();
  (void)usb_cdc_write(b, sizeof(b));
}

/**
 * @brief 处理一帧命令。
 * @param f 4 字节命令帧。
 * @return 无。
 */
static void handle_frame(const uint8_t *f)
{
  switch (f[0]) {
    case RAPFI_CMD_NEW:
      gomoku_new();
      s_turn = GOMOKU_BLACK;
      reply_ok();
      break;

    case RAPFI_CMD_PLAY:
      if (gomoku_place((int)f[1], (int)f[2], s_turn) == 0) {
        s_turn = 3 - s_turn;
        reply_ok();
      } else {
        reply_err();
      }
      break;

    case RAPFI_CMD_GO: {
      gomoku_result_t r;
      int depth = (f[1] == 0U) ? 6 : (int)f[1];
      uint32_t ms = (uint32_t)f[2] | ((uint32_t)f[3] << 8);
      if (gomoku_think(s_turn, depth, ms, &r) == 0) {
        s_turn = 3 - s_turn;
        reply_move(&r);
      } else {
        reply_err();
      }
      break;
    }

    case RAPFI_CMD_UNDO:
      if (gomoku_undo() == 0) {
        s_turn = 3 - s_turn;
        reply_ok();
      } else {
        reply_err();
      }
      break;

    case RAPFI_CMD_STATUS:
      reply_status();
      break;

    case RAPFI_CMD_TURN:
      reply_turn();
      break;

    case RAPFI_CMD_PING: {
      uint8_t b = RAPFI_RSP_READY;
      (void)usb_cdc_write(&b, 1U);
      break;
    }

    default:
      reply_err();
      break;
  }
}

void app_init(void)
{
  uint8_t ready = RAPFI_RSP_READY;

  gomoku_init();
  gomoku_new();
  s_turn = GOMOKU_BLACK;
  s_frame_len = 0U;
  (void)usb_cdc_write(&ready, 1U);
}

void app_poll(void)
{
  /* 半帧长时间未补全 -> 丢弃，重新同步。 */
  if ((s_frame_len != 0U) &&
      ((HAL_GetTick() - s_frame_tick) > FRAME_IDLE_TIMEOUT_MS)) {
    s_frame_len = 0U;
  }

  for (;;) {
    uint32_t got = usb_cdc_read(&s_frame[s_frame_len],
                                RAPFI_CMD_LEN - s_frame_len);
    if (got == 0U) {
      break;
    }
    s_frame_tick = HAL_GetTick();
    s_frame_len += got;
    if (s_frame_len == RAPFI_CMD_LEN) {
      handle_frame(s_frame);
      s_frame_len = 0U;
    }
  }
}
