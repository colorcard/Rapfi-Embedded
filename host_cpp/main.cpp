// Rapfi-Embedded TUI 上位机：用 FTXUI 在终端里与 STM32 五子棋引擎对战。
//
// MCU 协议（文本行，\r\n 结尾）：
//   NEW / PLAY x y / GO [depth] [ms] / UNDO / BOARD / STATUS / TURN
//   答：OK / ERR / MOVE x y score depth nodes time_ms / STATUS ...
//
// 键位：方向键或 hjkl 移动光标，Enter/空格 落子，鼠标点击落子，
//       n 新局，f 换先，u 悔棋，+/- 调深度，q 退出。
//
// 构建： cmake -S host_cpp -B host_cpp/build && cmake --build host_cpp/build
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <fcntl.h>
#include <glob.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;

static constexpr int kN = 15;
enum { kEmpty = 0, kBlack = 1, kWhite = 2 };

// 主题色
static const Color kWood = Color::RGB(198, 150, 92);
static const Color kStoneB = Color::RGB(120, 120, 130);
static const Color kStoneW = Color::RGB(245, 245, 250);
static const Color kCursor = Color::RGB(46, 74, 122);
static const Color kLast = Color::RGB(255, 196, 64);
static const Color kAccent = Color::RGB(120, 200, 255);

// ------------------------------ 串口 ------------------------------

class Serial {
 public:
  ~Serial() { close_port(); }

  bool open_port(const std::string& path) {
    close_port();
    fd_ = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) return false;
    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
      close_port();
      return false;
    }
    cfmakeraw(&tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
      close_port();
      return false;
    }
    return true;
  }

  void close_port() {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

  bool ok() const { return fd_ >= 0; }

  void write_line(const std::string& s) {
    if (fd_ < 0) return;
    std::string out = s + "\n";
    ssize_t n = ::write(fd_, out.data(), out.size());
    (void)n;
  }

  std::vector<std::string> read_lines() {
    std::vector<std::string> out;
    char tmp[1024];
    for (;;) {
      ssize_t n = ::read(fd_, tmp, sizeof(tmp));
      if (n <= 0) break;
      buf_.append(tmp, static_cast<size_t>(n));
    }
    size_t pos;
    while ((pos = buf_.find('\n')) != std::string::npos) {
      std::string line = buf_.substr(0, pos);
      buf_.erase(0, pos + 1);
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
      }
      if (!line.empty() && line[0] == '>') line.erase(0, 1);
      if (!line.empty()) out.push_back(line);
    }
    return out;
  }

  bool read_for(int ms, std::string* sink) {
    struct pollfd p{};
    p.fd = fd_;
    p.events = POLLIN;
    int r = ::poll(&p, 1, ms);
    if (r <= 0) return false;
    char tmp[256];
    ssize_t n = ::read(fd_, tmp, sizeof(tmp));
    if (n <= 0) return false;
    sink->append(tmp, static_cast<size_t>(n));
    return true;
  }

 private:
  int fd_ = -1;
  std::string buf_;
};

// ------------------------------ 引擎连接 ------------------------------

class Engine {
 public:
  Engine(Serial* ser, ScreenInteractive* screen)
      : ser_(ser), screen_(screen) {}

  void start() {
    alive_ = true;
    th_ = std::thread([this] {
      while (alive_) {
        auto lines = ser_->read_lines();
        if (!lines.empty()) {
          std::lock_guard<std::mutex> lk(m_);
          for (auto& s : lines) q_.push_back(s);
          screen_->PostEvent(Event::Custom);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    });
  }

  void stop() {
    alive_ = false;
    if (th_.joinable()) th_.join();
  }

  void send(const std::string& s) { ser_->write_line(s); }

  bool pop(std::string* out) {
    std::lock_guard<std::mutex> lk(m_);
    if (q_.empty()) return false;
    *out = q_.front();
    q_.pop_front();
    return true;
  }

 private:
  Serial* ser_;
  ScreenInteractive* screen_;
  std::thread th_;
  std::atomic<bool> alive_{false};
  std::mutex m_;
  std::deque<std::string> q_;
};

// ------------------------------ 对局 ------------------------------

struct Game {
  Engine* eng = nullptr;
  int human = kBlack;
  int depth = 6;
  int think_ms = 2000;
  int board[kN][kN] = {};
  int turn = kBlack;
  bool has_last = false;
  int last_x = 0;
  int last_y = 0;
  std::string info = "—";
  std::string status = "playing";
  std::string pending;  // "" / "PLAY" / "GO"

  int engine_side() const { return (human == kBlack) ? kWhite : kBlack; }

  void new_game(int h) {
    human = h;
    std::memset(board, 0, sizeof(board));
    turn = kBlack;
    has_last = false;
    info = "—";
    status = "playing";
    pending.clear();
    eng->send("NEW");
    if (turn == engine_side()) ask_engine();
  }

  void ask_engine() {
    pending = "GO";
    eng->send("GO " + std::to_string(depth) + " " + std::to_string(think_ms));
  }

  void click(int x, int y) {
    if (status != "playing" || !pending.empty()) return;
    if (turn != human) return;
    if (x < 0 || x >= kN || y < 0 || y >= kN) return;
    if (board[y][x] != kEmpty) return;
    pending = "PLAY";
    eng->send("PLAY " + std::to_string(x) + " " + std::to_string(y));
  }

  void undo() {
    if (!pending.empty()) return;
    eng->send("UNDO");
    eng->send("UNDO");
    eng->send("STATUS");
    std::memset(board, 0, sizeof(board));
    has_last = false;
    status = "playing";
    turn = human;
  }

  void poll() {
    std::string line;
    while (eng->pop(&line)) on_line(line);
  }

  void on_line(const std::string& line) {
    if (line == "OK") {
      if (pending == "PLAY") {
        pending.clear();
        turn = (turn == kBlack) ? kWhite : kBlack;
        eng->send("STATUS");
      }
      return;
    }
    if (line == "ERR") {
      pending.clear();
      return;
    }
    if (line.rfind("MOVE ", 0) == 0) {
      int x, y, score, dep;
      unsigned long nodes, ms;
      if (std::sscanf(line.c_str(), "MOVE %d %d %d %d %lu %lu", &x, &y, &score,
                      &dep, &nodes, &ms) == 6) {
        if (x >= 0 && x < kN && y >= 0 && y < kN && board[y][x] == kEmpty) {
          board[y][x] = engine_side();
          has_last = true;
          last_x = x;
          last_y = y;
        }
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "深度 %-2d  评估 %-8d\n%lu 节点   %lu ms", dep, score,
                      nodes, ms);
        info = buf;
      }
      turn = human;
      pending.clear();
      eng->send("STATUS");
      return;
    }
    if (line.rfind("STATUS ", 0) == 0) {
      std::string st = line.substr(7);
      if (st.rfind("WIN", 0) == 0) {
        int win = (st.find('B') != std::string::npos) ? kBlack : kWhite;
        status = (win == human) ? "human" : "engine";
      } else if (st == "DRAW") {
        status = "draw";
      } else if (st == "PLAYING") {
        if (pending.empty() && status == "playing" && turn == engine_side()) {
          ask_engine();
        }
      }
      return;
    }
  }
};

// ------------------------------ 渲染 ------------------------------

static const char* ColName(int x) {
  static const char* names[kN] = {"A", "B", "C", "D", "E", "F", "G", "H",
                                  "I", "J", "K", "L", "M", "N", "O"};
  return names[x];
}

// 交叉点字符：按是否处于上下/左右边界选择连接线。
static const char* GridChar(int x, int y) {
  bool top = (y == kN - 1);
  bool bot = (y == 0);
  bool left = (x == 0);
  bool right = (x == kN - 1);
  if (top && left) return "┌";
  if (top && right) return "┐";
  if (bot && left) return "└";
  if (bot && right) return "┘";
  if (top) return "┬";
  if (bot) return "┴";
  if (left) return "├";
  if (right) return "┤";
  return "┼";
}

// 棋盘：制表符网格，石子在交叉点；每交叉点占 3 列（字符 + 两格横线）。
static Element BoardElement(const Game& g, int cx, int cy) {
  Elements rows;

  auto coords = [&] {
    Elements r;
    r.push_back(text("   ") | color(kWood));
    for (int x = 0; x < kN; ++x) {
      r.push_back(text(std::string(" ") + ColName(x) + " ") | color(kWood));
    }
    r.push_back(text("  ") | color(kWood));
    return hbox(std::move(r));
  };

  rows.push_back(coords());
  for (int y = kN - 1; y >= 0; --y) {
    Elements r;
    char lbl[8];
    std::snprintf(lbl, sizeof(lbl), "%2d ", y + 1);
    r.push_back(text(lbl) | color(kWood));
    for (int x = 0; x < kN; ++x) {
      int v = g.board[y][x];
      const bool cur = (x == cx && y == cy);
      const bool last = (g.has_last && g.last_x == x && g.last_y == y);

      std::string ch = (v == kBlack) ? "●" : (v == kWhite) ? "○" : GridChar(x, y);
      const char* link = (x < kN - 1) ? "──" : "  ";

      Color ch_color = (v == kBlack)   ? kStoneB
                       : (v == kWhite) ? kStoneW
                                       : kWood;
      if (last) ch_color = kLast;

      Element ce = text(ch) | color(ch_color) | bold;
      Element de = text(link) | color(kWood);
      if (cur) {
        ce = ce | bgcolor(kCursor);
        de = de | bgcolor(kCursor);
      }
      r.push_back(ce);
      r.push_back(de);
    }
    char rl[8];
    std::snprintf(rl, sizeof(rl), " %2d", y + 1);
    r.push_back(text(rl) | color(kWood));
    rows.push_back(hbox(std::move(r)));
  }
  rows.push_back(coords());
  return vbox(std::move(rows));
}

static Element InfoPanel(const Game& g, int spin) {
  Elements v;
  Element side = (g.turn == g.human)
                     ? text("你") | bold | color(Color::GreenLight)
                     : text("引擎") | bold | color(Color::Cyan);
  v.push_back(hbox({text("轮到   ") | dim, side}) | center);

  if (g.pending == "GO") {
    static const char* kSpin = "|/-\\";
    v.push_back(hbox({text("思考中 ") | dim | color(Color::Yellow),
                      text(std::string(1, kSpin[spin % 4])) | bold |
                          color(Color::Yellow)}) |
                center);
  } else {
    v.push_back(text("") );
  }

  v.push_back(separator() | color(kWood));
  v.push_back(text("引擎分析") | dim);
  {
    std::string s = g.info;
    size_t p;
    while ((p = s.find('\n')) != std::string::npos) {
      v.push_back(text(s.substr(0, p)) | color(Color::RGB(200, 200, 200)));
      s.erase(0, p + 1);
    }
    v.push_back(text(s) | color(Color::RGB(200, 200, 200)));
  }

  v.push_back(separator() | color(kWood));
  if (g.has_last) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%s%d", ColName(g.last_x), g.last_y + 1);
    v.push_back(hbox({text("最后一手   ") | dim, text(buf) | bold}));
  } else {
    v.push_back(text("最后一手   —") | dim);
  }
  char d[32];
  std::snprintf(d, sizeof(d), "搜索深度   %d", g.depth);
  v.push_back(text(d));

  if (g.status != "playing") {
    v.push_back(separator() | color(kWood));
    std::string res = (g.status == "draw")      ? "平 局"
                      : (g.status == "human")   ? "你 赢 了 ！"
                                                : "引擎获胜";
    Color c = (g.status == "human") ? Color::GreenLight : Color::RedLight;
    v.push_back(text(res) | bold | color(c) | center);
  }
  return vbox(std::move(v));
}

int main(int argc, char** argv) {
  std::string port;
  int human = kBlack;
  int depth = 6;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--port" && i + 1 < argc) {
      port = argv[++i];
    } else if (a == "--side" && i + 1 < argc) {
      const char c = argv[++i][0];
      human = (c == 'w' || c == 'W') ? kWhite : kBlack;
    } else if (a == "--depth" && i + 1 < argc) {
      depth = std::atoi(argv[++i]);
    }
  }

  Serial ser;
  if (!port.empty()) {
    ser.open_port(port);
  } else {
    glob_t gl{};
    if (glob("/dev/cu.usbmodem*", 0, nullptr, &gl) == 0) {
      for (size_t i = 0; i < gl.gl_pathc && !ser.ok(); ++i) {
        if (!ser.open_port(gl.gl_pathv[i])) continue;
        ser.read_lines();
        ser.write_line("TURN");
        std::string resp;
        for (int k = 0; k < 6 && resp.find("TURN") == std::string::npos; ++k) {
          ser.read_for(100, &resp);
        }
        if (resp.find("TURN") == std::string::npos) ser.close_port();
      }
    }
    globfree(&gl);
  }
  if (!ser.ok()) {
    std::fprintf(stderr, "未找到 STM32 CDC 端口，请用 --port 指定\n");
    return 1;
  }

  auto screen = ScreenInteractive::Fullscreen();
  Engine eng(&ser, &screen);
  eng.start();

  Game game;
  game.eng = &eng;
  game.depth = depth;
  game.new_game(human);

  int cx = 7, cy = 7;
  int frame = 0;

  // 鼠标坐标映射：棋盘面板在左上角，外框 1 格；每交叉点 3 格宽。
  //   屏幕 x = 4 + 3*bx ；屏幕 y = 16 - by
  constexpr int kOriginX = 4;
  constexpr int kOriginY = 16;

  auto renderer = Renderer([&] {
    Element board = window(text(" Rapfi-Embedded ") | bold | color(kAccent),
                           BoardElement(game, cx, cy) | color(kWood)) |
                    size(HEIGHT, EQUAL, 17);
    Element info = window(text(" 对局 ") | color(kAccent),
                          InfoPanel(game, frame)) |
                   size(WIDTH, GREATER_THAN, 28) |
                   size(HEIGHT, EQUAL, 17) | flex;
    Element help =
        hbox({
            text(" 移动 ") | dim, text("↑↓←→/hjkl"),
            text("   落子 ") | dim, text("Enter / 鼠标"),
            text("   新局 ") | dim, text("n"),
            text("   换先 ") | dim, text("f"),
            text("   悔棋 ") | dim, text("u"),
            text("   思考 ") | dim, text("+/-"),
            text("   退出 ") | dim, text("q"),
        }) |
        center;
    return vbox({
               hbox({board, info}) | flex,
               help | border | color(kWood),
           }) |
           yflex_grow;
  });

  auto component = CatchEvent(renderer, [&](Event e) {
    if (e == Event::Custom) {
      game.poll();
      ++frame;
      return true;
    }
    if (e == Event::Character('q')) {
      screen.Exit();
      return true;
    }
    if (e == Event::Character('n')) {
      game.new_game(game.human);
      return true;
    }
    if (e == Event::Character('f')) {
      game.new_game(game.human == kBlack ? kWhite : kBlack);
      return true;
    }
    if (e == Event::Character('u')) {
      game.undo();
      return true;
    }
    if (e == Event::Character('+') || e == Event::Character('=')) {
      if (game.depth < 12) ++game.depth;
      return true;
    }
    if (e == Event::Character('-')) {
      if (game.depth > 1) --game.depth;
      return true;
    }
    if (e == Event::ArrowLeft || e == Event::Character('h')) {
      if (cx > 0) --cx;
      return true;
    }
    if (e == Event::ArrowRight || e == Event::Character('l')) {
      if (cx < kN - 1) ++cx;
      return true;
    }
    if (e == Event::ArrowUp || e == Event::Character('k')) {
      if (cy < kN - 1) ++cy;
      return true;
    }
    if (e == Event::ArrowDown || e == Event::Character('j')) {
      if (cy > 0) --cy;
      return true;
    }
    if (e == Event::Return || e == Event::Character(' ')) {
      game.click(cx, cy);
      return true;
    }
    if (e.is_mouse()) {
      if (e.mouse().button == Mouse::Left &&
          e.mouse().motion == Mouse::Pressed) {
        int bx = (e.mouse().x - kOriginX) / 3;
        int by = kOriginY - e.mouse().y;
        if (bx >= 0 && bx < kN && by >= 0 && by < kN) {
          cx = bx;
          cy = by;
          game.click(bx, by);
        }
      }
      return true;
    }
    return false;
  });

  std::atomic<bool> tick_alive{true};
  std::thread ticker([&] {
    while (tick_alive) {
      std::this_thread::sleep_for(std::chrono::milliseconds(80));
      screen.PostEvent(Event::Custom);
    }
  });

  screen.Loop(component);
  tick_alive = false;
  ticker.join();
  eng.stop();
  ser.close_port();
  return 0;
}
