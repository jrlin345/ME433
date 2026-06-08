import pygame
import serial
import serial.tools.list_ports
import sys
import math
import random

# ── Serial config ─────────────────────────────────────────────────────────────
BAUD_RATE     = 115200
THROTTLE_DEG  = 345   # degrees above which car accelerates

# ── Display config ────────────────────────────────────────────────────────────
W, H          = 900, 500
FPS           = 60

# ── Physics ───────────────────────────────────────────────────────────────────
MAX_SPEED     = 8.0
ACCEL         = 0.18
DECEL         = 0.12
CAR_Y         = H // 2 + 60

# ── Colors ────────────────────────────────────────────────────────────────────
SKY_TOP       = (15,  20,  40)
SKY_BOT       = (30,  50,  90)
ROAD_DARK     = (30,  30,  35)
ROAD_LINE     = (240, 220, 60)
GRASS_TOP     = (40,  90,  40)
GRASS_BOT     = (20,  60,  20)
CAR_BODY      = (220, 50,  50)
CAR_ROOF      = (180, 30,  30)
CAR_WINDOW    = (160, 210, 240)
WHEEL_COL     = (25,  25,  25)
HUD_BG        = (0,   0,   0,  160)
WHITE         = (255, 255, 255)
YELLOW        = (240, 220, 60)
RED           = (220, 60,  60)
GREEN         = (60,  200, 80)

# ─────────────────────────────────────────────────────────────────────────────
def find_pico_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if any(k in p.description.lower() for k in ["pico", "usb serial", "uart", "cdc"]):
            return p.device
    # fallback: first available
    return ports[0].device if ports else None

# ─────────────────────────────────────────────────────────────────────────────
def draw_background(surf, scroll):
    # Sky gradient
    for y in range(CAR_Y - 120):
        t = y / (CAR_Y - 120)
        r = int(SKY_TOP[0] + t * (SKY_BOT[0] - SKY_TOP[0]))
        g = int(SKY_TOP[1] + t * (SKY_BOT[1] - SKY_TOP[1]))
        b = int(SKY_TOP[2] + t * (SKY_BOT[2] - SKY_TOP[2]))
        pygame.draw.line(surf, (r, g, b), (0, y), (W, y))

    horizon_y = CAR_Y - 120

    # Grass
    for y in range(horizon_y, H):
        t = (y - horizon_y) / (H - horizon_y)
        r = int(GRASS_TOP[0] + t * (GRASS_BOT[0] - GRASS_TOP[0]))
        g = int(GRASS_TOP[1] + t * (GRASS_BOT[1] - GRASS_TOP[1]))
        b = int(GRASS_TOP[2] + t * (GRASS_BOT[2] - GRASS_TOP[2]))
        pygame.draw.line(surf, (r, g, b), (0, y), (W, y))

    # Road
    road_top_w = 200
    road_bot_w = W
    road_top_y = horizon_y
    road_bot_y = H

    road_pts = [
        (W//2 - road_top_w//2, road_top_y),
        (W//2 + road_top_w//2, road_top_y),
        (W,                    road_bot_y),
        (0,                    road_bot_y),
    ]
    pygame.draw.polygon(surf, ROAD_DARK, road_pts)

    # Dashed centre line (perspective scroll)
    num_dashes = 8
    for i in range(num_dashes):
        t0 = (i     / num_dashes + scroll * 0.003) % 1.0
        t1 = ((i + 0.5) / num_dashes + scroll * 0.003) % 1.0
        for ta, tb in [(t0, min(t0 + 0.04, 1.0))]:
            ya = int(road_top_y + ta * (road_bot_y - road_top_y))
            yb = int(road_top_y + tb * (road_bot_y - road_top_y))
            wa = int(road_top_w//2 * (1 - ta) * 0.05)
            xa = W // 2
            pygame.draw.line(surf, ROAD_LINE, (xa, ya), (xa, yb), max(1, int(4 * ta)))

# ─────────────────────────────────────────────────────────────────────────────
def draw_car(surf, x, y, speed):
    # Wheel spin angle based on speed
    wheel_r = 14

    # Body
    body_rect = pygame.Rect(x - 55, y - 22, 110, 40)
    pygame.draw.rect(surf, CAR_BODY, body_rect, border_radius=8)

    # Roof
    roof_pts = [(x - 30, y - 22), (x + 30, y - 22),
                (x + 22, y - 42), (x - 22, y - 42)]
    pygame.draw.polygon(surf, CAR_ROOF, roof_pts)

    # Window
    win_pts = [(x - 22, y - 24), (x + 22, y - 24),
               (x + 16, y - 40), (x - 16, y - 40)]
    pygame.draw.polygon(surf, CAR_WINDOW, win_pts)

    # Wheels
    for wx, wy in [(x - 35, y + 16), (x + 35, y + 16)]:
        pygame.draw.circle(surf, WHEEL_COL, (wx, wy), wheel_r)
        pygame.draw.circle(surf, (80, 80, 80), (wx, wy), wheel_r - 4)
        # Spoke
        angle = (pygame.time.get_ticks() * 0.01 * speed) % (2 * math.pi)
        for spoke in range(4):
            a = angle + spoke * math.pi / 2
            sx = wx + int(math.cos(a) * (wheel_r - 5))
            sy = wy + int(math.sin(a) * (wheel_r - 5))
            pygame.draw.line(surf, (60, 60, 60), (wx, wy), (sx, sy), 2)

    # Headlights
    pygame.draw.circle(surf, (255, 255, 180), (x + 52, y - 5), 5)
    pygame.draw.circle(surf, (255, 255, 180), (x - 52, y - 5), 5)

# ─────────────────────────────────────────────────────────────────────────────
def draw_hud(surf, pos, force, speed, throttle, font, small_font):
    # HUD panel
    hud = pygame.Surface((260, 110), pygame.SRCALPHA)
    hud.fill((0, 0, 0, 150))
    surf.blit(hud, (10, 10))

    # Speedometer bar
    spd_pct = speed / MAX_SPEED
    pygame.draw.rect(surf, (50, 50, 50), (20, 20, 180, 14), border_radius=7)
    bar_col  = GREEN if spd_pct < 0.6 else YELLOW if spd_pct < 0.85 else RED
    pygame.draw.rect(surf, bar_col, (20, 20, int(180 * spd_pct), 14), border_radius=7)
    surf.blit(small_font.render(f"SPD  {speed:.1f}", True, WHITE), (210, 18))

    # Paddle position
    surf.blit(small_font.render(f"POS  {pos}°", True, WHITE), (20, 44))
    surf.blit(small_font.render(f"FORCE {force}", True, WHITE), (20, 64))

    # Throttle indicator
    throttle_txt = "THROTTLE" if throttle else "COAST"
    col = GREEN if throttle else RED
    surf.blit(small_font.render(throttle_txt, True, col), (20, 90))

# ─────────────────────────────────────────────────────────────────────────────
def main():
    # ── Find and open serial port ─────────────────────────────────────────────
    port = find_pico_port()
    if port is None:
        print("No serial port found. Running in demo mode (random data).")
        ser = None
    else:
        try:
            ser = serial.Serial(port, BAUD_RATE, timeout=0)
            print(f"Connected to {port}")
        except Exception as e:
            print(f"Could not open {port}: {e}. Running in demo mode.")
            ser = None

    pygame.init()
    screen = pygame.display.set_mode((W, H))
    pygame.display.set_caption("Haptic Paddle Car")
    clock  = pygame.time.Clock()

    font       = pygame.font.SysFont("monospace", 28, bold=True)
    small_font = pygame.font.SysFont("monospace", 16)

    # State
    pos      = 0
    force    = 0
    speed    = 0.0
    scroll   = 0.0
    buf      = ""

    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit(); sys.exit()
            if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                pygame.quit(); sys.exit()

        # ── Read serial ───────────────────────────────────────────────────────
        if ser:
            try:
                buf += ser.read(ser.in_waiting or 1).decode("utf-8", errors="ignore")
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    parts = line.strip().split(",")
                    if len(parts) == 2:
                        pos   = int(parts[0].strip())
                        force = int(parts[1].strip())
            except Exception:
                pass
        else:
            # Demo mode: simulate paddle
            t   = pygame.time.get_ticks() / 1000
            pos = int(180 + 100 * math.sin(t * 0.5))

        # ── Physics ───────────────────────────────────────────────────────────
        throttle = pos >= THROTTLE_DEG
        if throttle:
            speed = min(speed + ACCEL, MAX_SPEED)
        else:
            speed = max(speed - DECEL, 0.0)

        scroll += speed

        # ── Draw ──────────────────────────────────────────────────────────────
        draw_background(screen, scroll)
        draw_car(screen, W // 2, CAR_Y, speed)
        draw_hud(screen, pos, force, speed, throttle, font, small_font)

        # Demo mode label
        if not ser:
            lbl = small_font.render("DEMO MODE — no serial", True, YELLOW)
            screen.blit(lbl, (W - lbl.get_width() - 10, 10))

        pygame.display.flip()
        clock.tick(FPS)

if __name__ == "__main__":
    main()