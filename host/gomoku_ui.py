#!/usr/bin/env python3
"""Rapfi-Embedded PC 上位机：五子棋棋盘 UI，通过 Type-C USB CDC 与 STM32 引擎对战。

MCU 侧协议（文本行，\\r\\n 结尾）：
    NEW                     新开一局，黑先行
    PLAY x y                当前方落子（x,y ∈ 0..14，y=0 在底部）
    GO [depth] [ms]         引擎为当前方思考并落子
    UNDO                    撤销一手
    BOARD                   文本棋盘
    STATUS                  返回 STATUS PLAYING / DRAW / WIN B|W
    TURN                    返回 TURN B|W
    → 应答：OK / ERR / MOVE x y score depth nodes time_ms / ...

用法：
    python3 host/gomoku_ui.py                 # 自动找 STM32 CDC 端口，人执黑
    python3 host/gomoku_ui.py --port /dev/cu.usbmodemXXXX
    python3 host/gomoku_ui.py --side w        # 人执白（引擎先行）
    python3 host/gomoku_ui.py --depth 6 --time 2000
    python3 host/gomoku_ui.py --selftest      # 无界面，跑几步验证协议

按键：N 新局 / F 换先 / U 悔棋 / +/- 深度 / Q 退出
"""
import argparse
import glob
import queue
import sys
import threading
import time

BOARD = 15
EMPTY, BLACK, WHITE = 0, 1, 2
DIRS = ((1, 0), (0, 1), (1, 1), (1, -1))


def find_port(explicit=None):
    if explicit:
        return explicit
    import serial

    for p in sorted(glob.glob("/dev/cu.usbmodem*")):
        try:
            s = serial.Serial(p, 115200, timeout=0.3)
            time.sleep(0.1)
            s.reset_input_buffer()
            s.write(b"TURN\n")
            time.sleep(0.3)
            resp = s.read(64)
            s.close()
            if b"TURN" in resp:
                return p
        except Exception:
            continue
    return None


class Engine:
    """串口封装：后台读线程 + 行队列 + 同步命令发送。"""

    def __init__(self, port):
        import serial

        self.ser = serial.Serial(port, 115200, timeout=0.05)
        self.lock = threading.Lock()
        self.lines = queue.Queue()
        self.buf = b""
        self.alive = True
        self.t = threading.Thread(target=self._reader, daemon=True)
        self.t.start()

    def _reader(self):
        while self.alive:
            try:
                data = self.ser.read(4096)
            except Exception:
                break
            if not data:
                continue
            self.buf += data
            while b"\n" in self.buf:
                raw, self.buf = self.buf.split(b"\n", 1)
                line = raw.decode(errors="replace").strip("\r").strip()
                if line.startswith(">"):
                    line = line[1:].strip()
                if line:
                    self.lines.put(line)

    def send(self, text):
        with self.lock:
            self.ser.write((text + "\n").encode())

    def close(self):
        self.alive = False
        try:
            self.ser.close()
        except Exception:
            pass


class Game:
    def __init__(self, eng, human_side, depth, think_ms):
        self.eng = eng
        self.human = human_side
        self.depth = depth
        self.think_ms = think_ms
        self.board = [[EMPTY] * BOARD for _ in range(BOARD)]
        self.turn = BLACK
        self.last = None
        self.status = "playing"
        self.info = ""
        self.pending = None
        self.pending_move = None
        self.new_game(human_side)

    @property
    def engine_side(self):
        return WHITE if self.human == BLACK else BLACK

    def new_game(self, human_side):
        self.human = human_side
        self.board = [[EMPTY] * BOARD for _ in range(BOARD)]
        self.turn = BLACK
        self.last = None
        self.status = "playing"
        self.info = ""
        self.pending = None
        self.pending_move = None
        self.eng.send("NEW")
        if self.turn == self.engine_side:
            self._ask_engine()

    def _ask_engine(self):
        self.pending = "GO"
        self.eng.send(f"GO {self.depth} {self.think_ms}")

    def click(self, x, y):
        if self.status != "playing" or self.pending is not None:
            return
        if self.turn != self.human:
            return
        if not (0 <= x < BOARD and 0 <= y < BOARD):
            return
        if self.board[y][x] != EMPTY:
            return
        self.pending = "PLAY"
        self.pending_move = (x, y)
        self.eng.send(f"PLAY {x} {y}")

    def undo(self):
        if self.pending is not None:
            return
        self.eng.send("UNDO")
        self.eng.send("UNDO")
        self.eng.send("STATUS")
        self.board = [[EMPTY] * BOARD for _ in range(BOARD)]
        self.last = None
        self.status = "playing"
        self.turn = self.human
        self.pending_move = None
        self.eng.send("BOARD")

    def poll(self):
        while True:
            try:
                line = self.eng.lines.get_nowait()
            except queue.Empty:
                return
            self._on_line(line)

    def _on_line(self, line):
        if line == "OK":
            if self.pending == "PLAY":
                self.pending = None
                if self.pending_move is not None:
                    x, y = self.pending_move
                    if self.board[y][x] == EMPTY:
                        self.board[y][x] = self.human
                        self.last = (x, y)
                    self.pending_move = None
                self.turn = WHITE if self.turn == BLACK else BLACK
                self.eng.send("STATUS")
            return
        if line == "ERR":
            self.pending = None
            self.pending_move = None
            return
        if line.startswith("MOVE "):
            parts = line.split()
            try:
                x, y = int(parts[1]), int(parts[2])
                score, depth = int(parts[3]), int(parts[4])
                nodes, ms = int(parts[5]), int(parts[6])
            except (IndexError, ValueError):
                return
            if self.board[y][x] == EMPTY:
                self.board[y][x] = self.engine_side
                self.last = (x, y)
            self.info = f"depth {depth}  score {score}  {nodes} nodes  {ms} ms"
            self.turn = self.human
            self.pending = None
            self.eng.send("STATUS")
            return
        if line.startswith("STATUS "):
            st = line[7:].strip()
            if st.startswith("WIN"):
                who = st.split()[1]
                win = BLACK if who == "B" else WHITE
                self.status = "human" if win == self.human else "engine"
            elif st == "DRAW":
                self.status = "draw"
            elif st == "PLAYING":
                if (self.pending is None) and (self.status == "playing") and \
                        (self.turn == self.engine_side):
                    self._ask_engine()
            return


def run_cli(game):
    """无界面自测：人随机开局，引擎应答，打印过程。"""
    import random

    for i in range(12):
        if game.status != "playing":
            break
        if game.turn == game.human:
            cand = [(x, y) for y in range(BOARD) for x in range(BOARD)
                    if game.board[y][x] == EMPTY and abs(x - 7) <= 4
                    and abs(y - 7) <= 4]
            if not cand:
                break
            x, y = random.choice(cand)
            print(f"human   {i+1:02d}: PLAY {x} {y}")
            game.click(x, y)
        deadline = time.time() + 8.0
        while time.time() < deadline:
            game.poll()
            if game.status != "playing":
                break
            if game.turn == game.human and game.pending is None:
                break
            time.sleep(0.05)
        print(f"        engine: {game.info}  last={game.last}")
    print("status:", game.status)


def run_gui(game):
    import pygame

    cell, margin = 34, 40
    size = margin * 2 + (BOARD - 1) * cell
    bar = 96
    pygame.init()
    screen = pygame.display.set_mode((size, size + bar))
    pygame.display.set_caption("Rapfi-Embedded Gomoku")
    font = pygame.font.SysFont("Menlo", 16)
    font_big = pygame.font.SysFont("Menlo", 20, bold=True)
    clock = pygame.time.Clock()

    def to_px(x, y):
        return margin + x * cell, margin + (BOARD - 1 - y) * cell

    def from_px(px, py):
        x = round((px - margin) / cell)
        y = BOARD - 1 - round((py - margin) / cell)
        return x, y

    running = True
    while running:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False
            elif ev.type == pygame.KEYDOWN:
                if ev.key in (pygame.K_q, pygame.K_ESCAPE):
                    running = False
                elif ev.key == pygame.K_n:
                    game.new_game(game.human)
                elif ev.key == pygame.K_f:
                    game.new_game(WHITE if game.human == BLACK else BLACK)
                elif ev.key == pygame.K_u:
                    game.undo()
                elif ev.key in (pygame.K_PLUS, pygame.K_EQUALS):
                    game.depth = min(12, game.depth + 1)
                elif ev.key == pygame.K_MINUS:
                    game.depth = max(1, game.depth - 1)
            elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                game.click(*from_px(*ev.pos))

        game.poll()

        screen.fill((232, 185, 106))
        for i in range(BOARD):
            a = margin
            b = margin + (BOARD - 1) * cell
            p = margin + i * cell
            pygame.draw.line(screen, (60, 40, 20), (a, p), (b, p), 1)
            pygame.draw.line(screen, (60, 40, 20), (p, a), (p, b), 1)
        for y in range(BOARD):
            for x in range(BOARD):
                v = game.board[y][x]
                if v == EMPTY:
                    continue
                px, py = to_px(x, y)
                r = cell // 2 - 3
                color = (20, 20, 20) if v == BLACK else (245, 245, 245)
                pygame.draw.circle(screen, color, (px, py), r)
                pygame.draw.circle(screen, (0, 0, 0), (px, py), r, 1)
        if game.last:
            px, py = to_px(*game.last)
            pygame.draw.circle(screen, (220, 40, 40), (px, py), 4)

        base = size
        if game.status == "playing":
            who = "你" if game.turn == game.human else "引擎"
            head = f"轮到 {who}" 
            if game.pending == "GO":
                head += "  (思考中...)"
        elif game.status == "draw":
            head = "平局"
        else:
            head = "你赢了！" if game.status == "human" else "引擎获胜"
        screen.blit(font_big.render(head, True, (30, 30, 30)), (16, base + 8))
        screen.blit(font.render(game.info, True, (60, 60, 60)), (16, base + 36))
        screen.blit(font.render(
            f"深度 {game.depth}   N 新局  F 换先  U 悔棋  +/- 深度  Q 退出",
            True, (90, 70, 40)), (16, base + 62))

        pygame.display.flip()
        clock.tick(60)
    pygame.quit()


def main():
    ap = argparse.ArgumentParser(description="Rapfi-Embedded Gomoku PC client")
    ap.add_argument("--port", default=None, help="串口，默认自动查找 STM32 CDC")
    ap.add_argument("--side", choices=["b", "w"], default="b", help="人类执子")
    ap.add_argument("--depth", type=int, default=6)
    ap.add_argument("--time", type=int, default=2000, help="引擎思考上限 ms")
    ap.add_argument("--selftest", action="store_true", help="无界面自测")
    args = ap.parse_args()

    port = find_port(args.port)
    if port is None:
        print("未找到 STM32 CDC 端口，请用 --port 指定", file=sys.stderr)
        return 1
    print(f"使用端口 {port}")
    eng = Engine(port)
    game = Game(eng, BLACK if args.side == "b" else WHITE, args.depth, args.time)
    try:
        if args.selftest:
            run_cli(game)
        else:
            run_gui(game)
    finally:
        eng.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
