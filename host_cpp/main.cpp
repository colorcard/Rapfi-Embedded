// Rapfi-Embedded TUI 上位机：用 FTXUI 在终端里与 STM32 五子棋引擎对战。
//
// MCU 协议（文本行，\r\n 结尾）：
//   NEW / PLAY x y / GO [depth] [ms] / UNDO / BOARD / STATUS / TURN
//   答：OK / ERR / MOVE x y score depth nodes time_ms / STATUS ...
//
// 键位：方向键或 hjkl 移动光标，Enter/空格 落子，n 新局，f 换先，u 悔棋，
//       +/- 调深度，q 退出。棋盘上点击也可落子（鼠标）。
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

  // 读取可用字节并切分成完整行。
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

  // 带超时的读取（用于端口探测）。
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
  std::string info;
  std::string status = "playing";
  std::string pending;  // "" / "PLAY" / "GO"

  int engine_side() const { return (human == kBlack) ? kWhite : kBlack; }

  void new_game(int h) {
    human = h;
    std::memset(board, 0, sizeof(board));
    turn = kBlack;
    has_last = false;
    info.clear();
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
        char buf[128];
        std::snprintf(buf, sizeof(buf), "depth %d  score %d  %lu nodes  %lu ms",
                      dep, score, nodes, ms);
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

static Element BoardElement(const Game& g, int cx, int cy) {
  Elements rows;
  {
    Elements h;
    h.push_back(text("  "));
    h.push_back(text(" "));
    for (int x = 0; x < kN; ++x) {
      h.push_back(text(std::string(" ") + static_cast<char>('A' + x)));
    }
    rows.push_back(hbox(std::move(h)));
  }
  for (int y = kN - 1; y >= 0; --y) {
    Elements r;
    char lbl[8];
    std::snprintf(lbl, sizeof(lbl), "%2d", y + 1);
    r.push_back(text(lbl));
    r.push_back(text(" "));
    for (int x = 0; x < kN; ++x) {
      int v = g.board[y][x];
      std::string s = (v == kBlack) ? " X" : (v == kWhite) ? " O" : " .";
      Element e = text(s);
      if (g.has_last && g.last_x == x && g.last_y == y) {
        e = e | color(Color::Red);
      }
      if (x == cx && y == cy) {
        e = e | inverted;
      }
      r.push_back(e);
    }
    rows.push_back(hbox(std::move(r)));
  }
  return vbox(std::move(rows));
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
        ser.read_lines();  // 清空
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
  // 棋盘在屏幕上的固定偏移（标题 1 行 + 分隔 1 行 + 列标题 1 行；列偏移 3）。
  constexpr int kBoardOriginY = 3;
  constexpr int kBoardOriginX = 3;

  auto renderer = Renderer([&] {
    std::string head;
    if (game.status == "playing") {
      head = (game.turn == game.human) ? "轮到 你" : "轮到 引擎";
      if (game.pending == "GO") head += "  (思考中...)";
    } else if (game.status == "draw") {
      head = "平局";
    } else {
      head = (game.status == "human") ? "你赢了！" : "引擎获胜";
    }
    std::string result;
    if (game.status != "playing") {
      result = (game.status == "draw") ? "= 平局 ="
               : (game.status == "human") ? "= 你赢了！ =" : "= 引擎获胜 =";
    }
    return vbox({
               text("Rapfi-Embedded Gomoku  (STM32G474)") | bold,
               separator(),
               BoardElement(game, cx, cy),
               separator(),
               text(head) | bold,
               text(game.info),
               result.empty() ? text("") : text(result) | bold | color(Color::Yellow),
               text("方向键/hjkl 移动  Enter 落子  鼠标点击  n 新局  f 换先  u "
                    "悔棋  +/- 深度  q 退出") |
                   dim,
           }) |
           yflex_grow;
  });

  auto component = CatchEvent(renderer, [&](Event e) {
    if (e == Event::Custom) {
      game.poll();
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
        int bx = (e.mouse().x - kBoardOriginX) / 2;
        int by = (kN - 1) - (e.mouse().y - kBoardOriginY);
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
