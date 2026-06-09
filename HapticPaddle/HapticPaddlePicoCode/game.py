"""
Haptic Paddle Wall Game
Reads position and desired_force from Pico over serial.
Serial format: "pos, desired_force\n"  e.g. "142, 87.3"

Usage:
    pip install pygame pyserial
    python haptic_game.py --port COM3        (Windows)
    python haptic_game.py --port /dev/ttyACM0  (Linux/Mac)
"""

import pygame
import serial
import threading
import argparse
import sys
import math
import time

# ── Config ───────────────────────────────────────────────────────────────────
WIDTH, HEIGHT = 900, 600
FPS = 60

WALL_START_POS = 130   # matches your Pico code
POS_MIN = 0
POS_MAX = 149

# ── Colours ──────────────────────────────────────────────────────────────────
BG          = (10,  12,  20)
TRACK_BG    = (25,  28,  45)
TRACK_EDGE  = (50,  55,  90)
FREE_COL    = (60, 180, 120)
WALL_COL    = (220, 70,  50)
PADDLE_COL  = (255, 220,  60)
FORCE_COL   = (100, 160, 255)
TEXT_COL    = (200, 210, 230)
DIM_COL     = (80,  90, 110)
GLOW_COL    = (255, 200,  40)

# ── Shared state (updated by serial thread) ───────────────────────────────────
state = {
    "pos":           0,
    "desired_force": 0.0,
    "connected":     False,
    "error":         "",
}
state_lock = threading.Lock()


def serial_reader(port, baud=115200):
    """Background thread: parse 'pos, desired_force\\n' lines from Pico."""
    try:
        ser = serial.Serial(port, baud, timeout=1)
        with state_lock:
            state["connected"] = True
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            parts = line.split(",")
            if len(parts) >= 2:
                try:
                    pos = int(parts[0].strip())
                    force = float(parts[1].strip())
                    with state_lock:
                        state["pos"] = max(POS_MIN, min(POS_MAX, pos))
                        state["desired_force"] = force
                except ValueError:
                    pass
    except serial.SerialException as e:
        with state_lock:
            state["error"] = str(e)


def lerp(a, b, t):
    return a + (b - a) * t


def draw_rounded_rect(surf, color, rect, radius):
    pygame.draw.rect(surf, color, rect, border_radius=radius)


def pos_to_x(pos, track_x, track_w):
    """Map 0-149 position to pixel x on the track."""
    return track_x + int((pos / POS_MAX) * track_w)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM4", help="Serial port (e.g. COM3 or /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--demo", action="store_true", help="Run without hardware (simulated paddle)")
    args = parser.parse_args()

    if not args.demo and args.port is None:
        args.port = "COM4"

    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("Haptic Paddle — Wall Demo")
    clock = pygame.time.Clock()

    font_large  = pygame.font.SysFont("Consolas", 42, bold=True)
    font_medium = pygame.font.SysFont("Consolas", 22)
    font_small  = pygame.font.SysFont("Consolas", 16)

    if not args.demo:
        t = threading.Thread(target=serial_reader, args=(args.port, args.baud), daemon=True)
        t.start()

    # demo sine wave
    demo_t = 0.0

    # smoothed values for rendering
    smooth_pos   = 0.0
    smooth_force = 0.0

    # force bar history for sparkline
    force_history = [0.0] * 120

    running = True
    while running:
        dt = clock.tick(FPS) / 1000.0

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                running = False

        # ── Get state ────────────────────────────────────────────────────────
        if args.demo:
            demo_t += dt
            # oscillate between 100 and 149 to show wall effect
            raw_pos = int(120 + 29 * abs(math.sin(demo_t * 0.4)))
            raw_force = max(0.0, (raw_pos - WALL_START_POS) / 19.0 * 150.0) if raw_pos > WALL_START_POS else 0.0
            cur_pos, cur_force = raw_pos, raw_force
            connected = True
            err = ""
        else:
            with state_lock:
                cur_pos   = state["pos"]
                cur_force = state["desired_force"]
                connected = state["connected"]
                err       = state["error"]

        smooth_pos   = lerp(smooth_pos,   float(cur_pos),   0.25)
        smooth_force = lerp(smooth_force, cur_force,         0.2)
        force_history.append(smooth_force)
        force_history.pop(0)

        in_wall = cur_pos > WALL_START_POS

        # ── Draw ─────────────────────────────────────────────────────────────
        screen.fill(BG)

        # title
        title = font_large.render("HAPTIC WALL", True, TEXT_COL)
        screen.blit(title, (WIDTH // 2 - title.get_width() // 2, 28))

        # ── Track ────────────────────────────────────────────────────────────
        TRACK_Y  = 200
        TRACK_H  = 60
        TRACK_X  = 80
        TRACK_W  = WIDTH - 160

        # background
        draw_rounded_rect(screen, TRACK_BG, (TRACK_X, TRACK_Y, TRACK_W, TRACK_H), 12)

        # free zone
        free_w = int((WALL_START_POS / POS_MAX) * TRACK_W)
        draw_rounded_rect(screen, (30, 70, 50), (TRACK_X, TRACK_Y, free_w, TRACK_H), 12)

        # wall zone — intensity based on penetration
        wall_x    = TRACK_X + free_w
        wall_w    = TRACK_W - free_w
        penetration = max(0.0, (cur_pos - WALL_START_POS) / (POS_MAX - WALL_START_POS))
        wall_alpha_col = (
            int(lerp(60, 220, penetration)),
            int(lerp(30, 70,  penetration)),
            int(lerp(30, 50,  penetration)),
        )
        draw_rounded_rect(screen, wall_alpha_col, (wall_x, TRACK_Y, wall_w, TRACK_H), 12)

        # wall start marker
        wall_marker_x = pos_to_x(WALL_START_POS, TRACK_X, TRACK_W)
        pygame.draw.line(screen, WALL_COL, (wall_marker_x, TRACK_Y - 10), (wall_marker_x, TRACK_Y + TRACK_H + 10), 2)
        wlabel = font_small.render("WALL", True, WALL_COL)
        screen.blit(wlabel, (wall_marker_x - wlabel.get_width() // 2, TRACK_Y - 28))

        # track border
        pygame.draw.rect(screen, TRACK_EDGE, (TRACK_X, TRACK_Y, TRACK_W, TRACK_H), 2, border_radius=12)

        # ── Paddle ───────────────────────────────────────────────────────────
        paddle_x = pos_to_x(smooth_pos, TRACK_X, TRACK_W)
        paddle_col = WALL_COL if in_wall else PADDLE_COL

        # glow
        if in_wall:
            glow_r = int(lerp(20, 55, penetration))
            glow_surf = pygame.Surface((glow_r * 2, glow_r * 2), pygame.SRCALPHA)
            pygame.draw.circle(glow_surf, (*WALL_COL, 60), (glow_r, glow_r), glow_r)
            screen.blit(glow_surf, (paddle_x - glow_r, TRACK_Y + TRACK_H // 2 - glow_r))

        pygame.draw.circle(screen, paddle_col, (paddle_x, TRACK_Y + TRACK_H // 2), 22)
        pygame.draw.circle(screen, BG,         (paddle_x, TRACK_Y + TRACK_H // 2), 10)

        # position label under paddle
        pos_label = font_small.render(f"{cur_pos}", True, paddle_col)
        screen.blit(pos_label, (paddle_x - pos_label.get_width() // 2, TRACK_Y + TRACK_H + 14))

        # ── Force bar ────────────────────────────────────────────────────────
        BAR_X, BAR_Y, BAR_W, BAR_H = 80, 320, TRACK_W, 28
        max_display_force = 160.0

        draw_rounded_rect(screen, TRACK_BG, (BAR_X, BAR_Y, BAR_W, BAR_H), 6)
        fill_w = int(min(1.0, smooth_force / max_display_force) * BAR_W)
        if fill_w > 0:
            draw_rounded_rect(screen, FORCE_COL, (BAR_X, BAR_Y, fill_w, BAR_H), 6)
        pygame.draw.rect(screen, TRACK_EDGE, (BAR_X, BAR_Y, BAR_W, BAR_H), 2, border_radius=6)

        bar_label = font_small.render("DESIRED FORCE", True, DIM_COL)
        screen.blit(bar_label, (BAR_X, BAR_Y - 20))
        bar_val = font_small.render(f"{smooth_force:.1f}", True, FORCE_COL)
        screen.blit(bar_val, (BAR_X + BAR_W + 10, BAR_Y + 6))

        # ── Sparkline ────────────────────────────────────────────────────────
        SPARK_X, SPARK_Y, SPARK_W, SPARK_H = 80, 390, TRACK_W, 80
        draw_rounded_rect(screen, TRACK_BG, (SPARK_X, SPARK_Y, SPARK_W, SPARK_H), 8)
        if len(force_history) > 1:
            pts = []
            for i, fv in enumerate(force_history):
                x = SPARK_X + int(i / len(force_history) * SPARK_W)
                y = SPARK_Y + SPARK_H - int(min(1.0, fv / max_display_force) * SPARK_H)
                pts.append((x, y))
            pygame.draw.lines(screen, FORCE_COL, False, pts, 2)
        pygame.draw.rect(screen, TRACK_EDGE, (SPARK_X, SPARK_Y, SPARK_W, SPARK_H), 1, border_radius=8)
        slabel = font_small.render("FORCE HISTORY", True, DIM_COL)
        screen.blit(slabel, (SPARK_X, SPARK_Y - 20))

        # ── Status panel ─────────────────────────────────────────────────────
        status_y = 500
        if not args.demo:
            if err:
                s = font_medium.render(f"⚠ Serial error: {err}", True, WALL_COL)
            elif not connected:
                s = font_medium.render("Connecting to Pico...", True, DIM_COL)
            else:
                s = font_medium.render(f"● {args.port}  {args.baud} baud", True, FREE_COL)
            screen.blit(s, (WIDTH // 2 - s.get_width() // 2, status_y))
        else:
            s = font_medium.render("DEMO MODE — no hardware", True, DIM_COL)
            screen.blit(s, (WIDTH // 2 - s.get_width() // 2, status_y))

        # wall hit flash
        if in_wall:
            label = font_medium.render("▐ WALL CONTACT ▌", True, WALL_COL)
            screen.blit(label, (WIDTH // 2 - label.get_width() // 2, 150))

        hint = font_small.render("ESC to quit", True, DIM_COL)
        screen.blit(hint, (WIDTH - hint.get_width() - 20, HEIGHT - 28))

        pygame.display.flip()

    pygame.quit()


if __name__ == "__main__":
    main()