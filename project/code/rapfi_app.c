#include "rapfi_app.h"

#include <stdio.h>

#include "gomoku.h"
#include "zf_device_usb_cdc.h"

/** @brief 单行命令最大长度。 */
#define LINE_MAX      160U
/** @brief 棋盘文本缓冲大小。 */
#define BOARD_TEXT_SZ 600U
/** @brief 默认搜索深度。 */
#define DEFAULT_DEPTH 6
/** @brief 默认搜索时间上限（毫秒）。 */
#define DEFAULT_TIME  3000U

static char s_line[LINE_MAX];
static uint32_t s_line_len;
static int s_turn;

/**
 * @brief 大小写不敏感的字符串比较。
 * @param a 字符串一。
 * @param b 字符串二。
 * @return 相等返回 1，否则 0。
 */
static int ci_equal(const char *a, const char *b)
{
  while ((*a != '\0') && (*b != '\0')) {
    char ca = *a;
    char cb = *b;
    if ((ca >= 'a') && (ca <= 'z')) {
      ca = (char)(ca - 'a' + 'A');
    }
    if ((cb >= 'a') && (cb <= 'z')) {
      cb = (char)(cb - 'a' + 'A');
    }
    if (ca != cb) {
      return 0;
    }
    ++a;
    ++b;
  }
  return (*a == '\0') && (*b == '\0');
}

/**
 * @brief 处理一条文本命令。
 * @param line 命令行（以 '\0' 结尾）。
 * @return 无。
 */
static void handle_line(const char *line)
{
  char cmd[16];
  static char text[BOARD_TEXT_SZ];
  int x;
  int y;

  if (sscanf(line, "%15s", cmd) != 1) {
    return;
  }

  if (ci_equal(cmd, "NEW") != 0) {
    gomoku_new();
    s_turn = GOMOKU_BLACK;
    usb_cdc_write_str("OK\r\n");
  } else if (ci_equal(cmd, "BOARD") != 0) {
    (void)gomoku_to_text(text, (int)sizeof(text));
    (void)usb_cdc_write_str(text);
  } else if (ci_equal(cmd, "PLAY") != 0) {
    if ((sscanf(line, "%*s %d %d", &x, &y) == 2) &&
        (gomoku_place(x, y, s_turn) == 0)) {
      s_turn = 3 - s_turn;
      usb_cdc_write_str("OK\r\n");
    } else {
      usb_cdc_write_str("ERR\r\n");
    }
  } else if (ci_equal(cmd, "GO") != 0) {
    gomoku_result_t r;
    int depth = DEFAULT_DEPTH;
    unsigned int time_ms = DEFAULT_TIME;
    (void)sscanf(line, "%*s %d %u", &depth, &time_ms);
    if (gomoku_think(s_turn, depth, time_ms, &r) == 0) {
      s_turn = 3 - s_turn;
      usb_cdc_printf("MOVE %d %d %d %d %lu %lu\r\n", r.x, r.y, r.score,
                     r.depth, (unsigned long)r.nodes, (unsigned long)r.time_ms);
    } else {
      usb_cdc_write_str("ERR\r\n");
    }
  } else if (ci_equal(cmd, "UNDO") != 0) {
    if (gomoku_undo() == 0) {
      s_turn = 3 - s_turn;
      usb_cdc_write_str("OK\r\n");
    } else {
      usb_cdc_write_str("ERR\r\n");
    }
  } else if (ci_equal(cmd, "TURN") != 0) {
    usb_cdc_printf("TURN %c\r\n", (s_turn == GOMOKU_BLACK) ? 'B' : 'W');
  } else if (ci_equal(cmd, "STATUS") != 0) {
    int st = gomoku_status();
    if (st == GOMOKU_EMPTY) {
      usb_cdc_write_str("STATUS PLAYING\r\n");
    } else if (st == GOMOKU_DRAW) {
      usb_cdc_write_str("STATUS DRAW\r\n");
    } else {
      usb_cdc_printf("STATUS WIN %c\r\n",
                     (st == GOMOKU_BLACK) ? 'B' : 'W');
    }
  } else {
    usb_cdc_write_str("ERR\r\n");
  }
}

void app_init(void)
{
  gomoku_init();
  gomoku_new();
  s_turn = GOMOKU_BLACK;
  s_line_len = 0U;
  usb_cdc_write_str("\r\nRapfi-Embedded ready.\r\n> ");
}

void app_poll(void)
{
  uint8_t byte;

  while (usb_cdc_try_read_byte(&byte) != 0) {
    if (byte == (uint8_t)'\n') {
      s_line[s_line_len] = '\0';
      handle_line(s_line);
      s_line_len = 0U;
    } else if ((byte != (uint8_t)'\r') && (byte != 0U)) {
      if (s_line_len < (LINE_MAX - 1U)) {
        s_line[s_line_len] = (char)byte;
        ++s_line_len;
      } else {
        s_line_len = 0U; /* 行过长丢弃 */
      }
    }
  }
}
