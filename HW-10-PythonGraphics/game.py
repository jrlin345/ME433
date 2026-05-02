"""
TURBO DASH — Racing game with Raspberry Pi Pico serial input
============================================================
C firmware sends:  B:1 P:0.7523\n
  B = button (0 or 1), P = potentiometer normalized (0.0–1.0)

Usage:
  python turbo_dash.py              # auto-detects Pico port
  python turbo_dash.py COM3         # Windows explicit port
  python turbo_dash.py /dev/ttyACM0 # Linux/Mac explicit port

Keyboard fallback (always active):
  SPACE      = jump
  LEFT/RIGHT = decrease/increase speed
  ESC        = quit
"""

import sys
import math
import random
import threading
import serial
import serial.tools.list_ports
import pygame
from dataclasses import dataclass
from typing import List

# ─── Palette ──────────────────────────────────────────────────────────────────
C_BG        = (10,  12,  20)
C_ROAD      = (22,  26,  38)
C_ROAD_LINE = (40,  46,  60)
C_LANE      = (255, 200,  40)
C_PLAYER    = (255,  80,  40)
C_OBSTACLE  = (50,  200, 255)
C_COIN      = (255, 210,  60)
C_BOOST     = (80,  255, 160)
C_WHITE     = (255, 255, 255)
C_GREY      = (120, 130, 150)
C_RED       = (255,  60,  60)
C_GREEN     = (80,  220, 120)

# ─── Config ───────────────────────────────────────────────────────────────────
W, H          = 900, 500
FPS           = 60
ROAD_TOP      = 180
ROAD_BOT      = H - 40
GROUND_Y      = ROAD_BOT - 60
JUMP_VEL      = -16
GRAVITY       = 0.7
BASE_SPEED    = 5.0
MAX_SPEED     = 18.0
LANE_COUNT    = 3
SERIAL_BAUD   = 115200


# ─── Particle ─────────────────────────────────────────────────────────────────
@dataclass
class Particle:
    x: float; y: float
    vx: float; vy: float
    life: float; max_life: float
    color: tuple; size: float

    def update(self):
        self.x  += self.vx
        self.y  += self.vy
        self.vy += 0.15
        self.life -= 1

    @property
    def alive(self): return self.life > 0

    def draw(self, surf):
        alpha = int(255 * (self.life / self.max_life))
        r = max(1, int(self.size * (self.life / self.max_life)))
        s = pygame.Surface((r * 2, r * 2), pygame.SRCALPHA)
        pygame.draw.circle(s, (*self.color[:3], alpha), (r, r), r)
        surf.blit(s, (self.x - r, self.y - r))


# ─── Obstacle ─────────────────────────────────────────────────────────────────
@dataclass
class Obstacle:
    x: float
    lane: int
    kind: str   # 'block' | 'barrier' | 'spike'
    w: int = 36
    h: int = 36

    @property
    def y(self):
        lane_h = (ROAD_BOT - ROAD_TOP) / LANE_COUNT
        return ROAD_TOP + lane_h * (self.lane + 0.5) - self.h // 2 + 20

    def rect(self):
        return pygame.Rect(self.x, self.y, self.w, self.h)

    def draw(self, surf):
        r = self.rect()
        if self.kind == 'block':
            pygame.draw.rect(surf, C_OBSTACLE, r, border_radius=4)
            pygame.draw.rect(surf, C_WHITE, r, 2, border_radius=4)
            m = 6
            pygame.draw.line(surf, C_WHITE, (r.x+m, r.y+m),    (r.right-m, r.bottom-m), 2)
            pygame.draw.line(surf, C_WHITE, (r.right-m, r.y+m), (r.x+m,    r.bottom-m), 2)
        elif self.kind == 'barrier':
            pygame.draw.rect(surf, C_RED, r, border_radius=2)
            for i in range(0, self.h, 8):
                pygame.draw.line(surf, (200, 30, 30), (r.x, r.y+i), (r.right, r.y+i), 1)
        else:  # spike
            pts = [(r.centerx, r.y), (r.right, r.bottom), (r.x, r.bottom)]
            pygame.draw.polygon(surf, C_OBSTACLE, pts)
            pygame.draw.polygon(surf, C_WHITE,    pts, 2)


# ─── Coin ─────────────────────────────────────────────────────────────────────
@dataclass
class Coin:
    x: float
    lane: int
    collected: bool = False

    @property
    def y(self):
        lane_h = (ROAD_BOT - ROAD_TOP) / LANE_COUNT
        return ROAD_TOP + lane_h * (self.lane + 0.5) + 20

    def rect(self):
        return pygame.Rect(self.x - 10, self.y - 10, 20, 20)

    def draw(self, surf, t):
        bob = math.sin(t * 0.05 + self.x * 0.1) * 3
        cx, cy = int(self.x), int(self.y + bob)
        pygame.draw.circle(surf, C_COIN, (cx, cy), 10)
        pygame.draw.circle(surf, (255, 240, 120), (cx-3, cy-3), 4)
        pygame.draw.circle(surf, C_COIN, (cx, cy), 10, 2)


# ─── Player ───────────────────────────────────────────────────────────────────
class Player:
    def __init__(self):
        self.x          = 120
        self.y          = float(GROUND_Y)
        self.vy         = 0.0
        self.on_ground  = True
        self.jump_count = 0
        self.trail:     List[tuple]    = []
        self.particles: List[Particle] = []
        self.w       = 34
        self.h       = 44
        self.squash  = 1.0
        self.stretch = 1.0

    def jump(self):
        if self.jump_count < 2:
            self.vy         = JUMP_VEL if self.jump_count == 0 else JUMP_VEL * 0.8
            self.jump_count += 1
            self.on_ground  = False
            self.stretch    = 0.6
            for _ in range(12):
                self.particles.append(Particle(
                    self.x + self.w / 2, self.y + self.h,
                    random.uniform(-3, 3), random.uniform(-5, -1),
                    20, 20, C_PLAYER, random.uniform(3, 7)
                ))

    def update(self, speed):
        self.vy += GRAVITY
        self.y  += self.vy
        if self.y >= GROUND_Y:
            self.y = GROUND_Y
            if not self.on_ground:
                self.squash = 1.4
                for _ in range(8):
                    self.particles.append(Particle(
                        self.x + self.w / 2, self.y + self.h,
                        random.uniform(-4, 4), random.uniform(-2, 1),
                        12, 12, C_PLAYER, random.uniform(2, 5)
                    ))
            self.vy         = 0
            self.on_ground  = True
            self.jump_count = 0

        self.squash  += (1.0 - self.squash)  * 0.2
        self.stretch += (1.0 - self.stretch) * 0.2

        self.trail.append((self.x + self.w / 2, self.y + self.h / 2))
        if len(self.trail) > 18:
            self.trail.pop(0)

        if random.random() < 0.4:
            self.particles.append(Particle(
                self.x, self.y + self.h * 0.6,
                -speed * 0.3 + random.uniform(-1, 1),
                random.uniform(-1, 1),
                10, 10, (255, 140, 40), random.uniform(2, 5)
            ))

        for p in self.particles: p.update()
        self.particles = [p for p in self.particles if p.alive]

    def rect(self):
        sw = int(self.w * self.squash)
        sh = int(self.h * self.stretch)
        return pygame.Rect(self.x + (self.w - sw) // 2, self.y + (self.h - sh), sw, sh)

    def draw(self, surf):
        # motion trail
        for i, (tx, ty) in enumerate(self.trail):
            alpha = int(180 * i / len(self.trail))
            r = max(1, int(8 * i / len(self.trail)))
            s = pygame.Surface((r * 2, r * 2), pygame.SRCALPHA)
            pygame.draw.circle(s, (*C_PLAYER, alpha), (r, r), r)
            surf.blit(s, (int(tx) - r, int(ty) - r))

        for p in self.particles:
            p.draw(surf)

        r = self.rect()
        # glow
        gs = pygame.Surface((r.w + 20, r.h + 20), pygame.SRCALPHA)
        pygame.draw.rect(gs, (*C_PLAYER, 40), gs.get_rect(), border_radius=10)
        surf.blit(gs, (r.x - 10, r.y - 10))
        # body
        pygame.draw.rect(surf, C_PLAYER, r, border_radius=6)
        # windshield
        ws = pygame.Rect(r.x + 4, r.y + 4, r.w - 8, r.h // 2 - 2)
        pygame.draw.rect(surf, (180, 230, 255), ws, border_radius=3)
        # wheels
        for wx, wy in [(r.x + 5, r.bottom - 2), (r.right - 5, r.bottom - 2)]:
            pygame.draw.circle(surf, (30, 30, 40), (wx, wy), 7)
            pygame.draw.circle(surf, C_GREY,       (wx, wy), 7, 2)
        # headlight
        pygame.draw.circle(surf, C_COIN, (r.right - 3, r.centery + 4), 4)


# ─── Serial reader ────────────────────────────────────────────────────────────
def find_pico_port() -> str | None:
    """Auto-detect Raspberry Pi Pico by USB vendor ID (2E8A)."""
    for port in serial.tools.list_ports.comports():
        hwid = (port.hwid or "").lower()
        desc = (port.description or "").lower()
        if "2e8a" in hwid or "pico" in desc or "raspberry" in desc:
            return port.device
    return None


def serial_thread(game: "Game", port: str):
    """Background thread: reads serial lines and drives game inputs."""
    print(f"[serial] Connecting to {port} at {SERIAL_BAUD} baud...")
    try:
        ser = serial.Serial(port, SERIAL_BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"[serial] ERROR — {e}")
        print("[serial] Falling back to keyboard-only mode.")
        return

    print("[serial] Connected.")
    prev_button = 0

    while True:
        try:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="ignore").strip()
            # Expected format: (1, 0.7523)

            if line.startswith("(") and line.endswith(")"):
                try:
                    content = line[1:-1]  # remove parentheses
                    button_str, volt_str = content.split(",")

                    button = int(button_str.strip())
                    volt   = float(volt_str.strip())

                    # Set speed (0.0–1.0)
                    game.set_speed(volt)

                    # Rising edge detection for jump
                    if button == 1 and prev_button == 0:
                        game.trigger_jump()

                    prev_button = button

                except (ValueError, IndexError):
                    # Ignore malformed lines
                    pass

        except (serial.SerialException, OSError):
            print("[serial] Connection lost.")
            break

# ─── Game ─────────────────────────────────────────────────────────────────────
class Game:
    def __init__(self):
        pygame.init()
        pygame.display.set_caption("TURBO DASH")
        self.screen = pygame.display.set_mode((W, H))
        self.clock  = pygame.time.Clock()

        self.font_big   = pygame.font.SysFont("consolas", 52, bold=True)
        self.font_med   = pygame.font.SysFont("consolas", 28, bold=True)
        self.font_small = pygame.font.SysFont("consolas", 18)

        # Serial state (written by serial thread, read by game thread)
        self._speed_input  = 0.5   # 0.0–1.0 from potentiometer
        self._jump_queued  = False
        self._serial_lock  = threading.Lock()

        self._reset()

    # ── Serial input interface (called from serial thread) ────────────────────
    def set_speed(self, value: float):
        with self._serial_lock:
            self._speed_input = max(0.0, min(1.0, float(value)))

    def trigger_jump(self):
        with self._serial_lock:
            self._jump_queued = True

    def _consume_inputs(self) -> tuple[float, bool]:
        with self._serial_lock:
            speed = self._speed_input
            jump  = self._jump_queued
            self._jump_queued = False
        return speed, jump

    # ── Reset ─────────────────────────────────────────────────────────────────
    def _reset(self):
        self.player           = Player()
        self.obstacles:       List[Obstacle] = []
        self.coins:           List[Coin]     = []
        self.particles:       List[Particle] = []
        self.speed            = BASE_SPEED
        self.target_speed     = BASE_SPEED
        self.score            = 0
        self.coins_collected  = 0
        self.distance         = 0.0
        self.t                = 0
        self.state            = "playing"   # playing | dead
        self.spawn_timer      = 0
        self.coin_timer       = 0
        self.road_offset      = 0.0
        self.bg_scroll        = 0.0
        self.flash            = 0
        self.stars = [
            (random.randint(0, W), random.randint(0, ROAD_TOP + 30), random.uniform(0.5, 2.0))
            for _ in range(80)
        ]

    # ── Spawning ──────────────────────────────────────────────────────────────
    def _spawn(self):
        self.spawn_timer += 1
        interval = max(35, int(90 - self.distance / 400))
        if self.spawn_timer >= interval:
            self.spawn_timer = 0
            self.obstacles.append(Obstacle(
                W + 20,
                random.randint(0, LANE_COUNT - 1),
                random.choice(['block', 'barrier', 'spike', 'block'])
            ))

        self.coin_timer += 1
        if self.coin_timer >= 45:
            self.coin_timer = 0
            self.coins.append(Coin(W + 40, random.randint(0, LANE_COUNT - 1)))

    # ── Road ──────────────────────────────────────────────────────────────────
    def _draw_road(self):
        # sky gradient
        for y in range(ROAD_TOP):
            t = y / ROAD_TOP
            pygame.draw.line(self.screen,
                (int(10 + t*12), int(12 + t*14), int(20 + t*18)), (0, y), (W, y))

        # stars
        for sx, sy, sz in self.stars:
            bx = (sx - self.bg_scroll * sz * 0.3) % W
            b  = int(100 + sz * 60)
            pygame.draw.circle(self.screen, (b, b, b + 20), (int(bx), sy), max(1, int(sz)))

        # road surface
        pygame.draw.rect(self.screen, C_ROAD, (0, ROAD_TOP, W, ROAD_BOT - ROAD_TOP))

        # lane dividers
        lane_h = (ROAD_BOT - ROAD_TOP) / LANE_COUNT
        for i in range(1, LANE_COUNT):
            y = int(ROAD_TOP + lane_h * i)
            pygame.draw.line(self.screen, C_ROAD_LINE, (0, y), (W, y), 1)

        # animated centre dashes
        dash_len, gap = 60, 30
        self.road_offset = (self.road_offset + self.speed) % (dash_len + gap)
        for x in range(int(-self.road_offset), W, dash_len + gap):
            pygame.draw.rect(self.screen, C_LANE, (x, ROAD_BOT - 3, dash_len, 3))

        # road edges
        pygame.draw.line(self.screen, C_LANE, (0, ROAD_TOP), (W, ROAD_TOP), 3)
        pygame.draw.line(self.screen, C_LANE, (0, ROAD_BOT), (W, ROAD_BOT), 3)

    # ── HUD ───────────────────────────────────────────────────────────────────
    def _draw_hud(self):
        # speed bar
        bar_w, bar_h = 160, 14
        bx, by = W - bar_w - 20, 20
        bg = pygame.Surface((bar_w + 4, bar_h + 4), pygame.SRCALPHA)
        pygame.draw.rect(bg, (0, 0, 0, 140), bg.get_rect(), border_radius=4)
        self.screen.blit(bg, (bx - 2, by - 2))
        fill = int(bar_w * ((self.speed - BASE_SPEED) / (MAX_SPEED - BASE_SPEED)))
        col  = (C_GREEN if self.speed < MAX_SPEED * 0.7
                else C_COIN if self.speed < MAX_SPEED * 0.9 else C_RED)
        if fill > 0:
            pygame.draw.rect(self.screen, col, (bx, by, fill, bar_h), border_radius=3)
        pygame.draw.rect(self.screen, C_GREY, (bx, by, bar_w, bar_h), 1, border_radius=3)
        self.screen.blit(
            self.font_small.render(f"SPEED {self.speed:.1f}", True, C_WHITE),
            (bx, by + bar_h + 4)
        )

        # score
        self.screen.blit(self.font_med.render(f"{int(self.score):06d}", True, C_WHITE), (20, 16))
        self.screen.blit(self.font_small.render("SCORE", True, C_GREY), (20, 48))

        # coin counter
        pygame.draw.circle(self.screen, C_COIN, (22, 80), 8)
        self.screen.blit(
            self.font_small.render(f"x {self.coins_collected}", True, C_COIN), (35, 72))

        # distance
        dm = self.font_small.render(f"{int(self.distance)}m", True, C_GREY)
        self.screen.blit(dm, (W // 2 - dm.get_width() // 2, 16))

        # jump pips (double jump indicator)
        for i in range(2):
            filled = i < (2 - self.player.jump_count)
            pygame.draw.circle(self.screen,
                C_PLAYER if filled else (40, 40, 60),
                (W // 2 - 12 + i * 28, 42), 8)
            pygame.draw.circle(self.screen, C_WHITE, (W // 2 - 12 + i * 28, 42), 8, 1)

    # ── Death screen ──────────────────────────────────────────────────────────
    def _draw_death(self):
        overlay = pygame.Surface((W, H), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 160))
        self.screen.blit(overlay, (0, 0))

        self.screen.blit(
            self.font_big.render("GAME OVER", True, C_RED),
            (W // 2 - self.font_big.size("GAME OVER")[0] // 2, H // 2 - 90))

        sc = self.font_med.render(f"SCORE  {int(self.score):06d}", True, C_WHITE)
        self.screen.blit(sc, (W // 2 - sc.get_width() // 2, H // 2 - 20))

        info = self.font_small.render(
            f"DISTANCE  {int(self.distance)} m     COINS  {self.coins_collected}",
            True, C_GREY)
        self.screen.blit(info, (W // 2 - info.get_width() // 2, H // 2 + 20))

        pulse   = abs(math.sin(self.t * 0.05))
        restart = self.font_med.render("[ SPACE ]  RESTART", True, C_COIN)
        rs      = pygame.Surface(restart.get_size(), pygame.SRCALPHA)
        rs.blit(restart, (0, 0))
        rs.set_alpha(int(120 + 135 * pulse))
        self.screen.blit(rs, (W // 2 - restart.get_width() // 2, H // 2 + 65))

    # ── Update ────────────────────────────────────────────────────────────────
    def update(self):
        self.t         += 1
        self.bg_scroll += self.speed * 0.5

        # consume serial inputs (thread-safe)
        speed_input, jump_queued = self._consume_inputs()

        # keyboard fallback for speed
        keys = pygame.key.get_pressed()
        if keys[pygame.K_RIGHT]:
            speed_input = min(1.0, speed_input + 0.01)
            self.set_speed(speed_input)
        if keys[pygame.K_LEFT]:
            speed_input = max(0.0, speed_input - 0.01)
            self.set_speed(speed_input)

        self.target_speed = BASE_SPEED + speed_input * (MAX_SPEED - BASE_SPEED)
        self.speed       += (self.target_speed - self.speed) * 0.05

        if jump_queued:
            self.player.jump()

        self.player.update(self.speed)
        self.distance += self.speed * 0.02
        self.score    += self.speed * 0.1

        self._spawn()

        # scroll obstacles & coins
        for obs in self.obstacles: obs.x -= self.speed
        for c   in self.coins:       c.x -= self.speed
        self.obstacles = [o for o in self.obstacles if o.x > -80]
        self.coins     = [c for c in self.coins     if c.x > -40]

        # collision — obstacles
        pr = self.player.rect().inflate(-8, -8)
        for obs in self.obstacles:
            if pr.colliderect(obs.rect()):
                self.flash = 12
                self.state = "dead"
                for _ in range(30):
                    self.particles.append(Particle(
                        pr.centerx, pr.centery,
                        random.uniform(-8, 8), random.uniform(-10, 2),
                        30, 30,
                        random.choice([C_PLAYER, C_COIN, C_WHITE]),
                        random.uniform(3, 9)
                    ))
                return

        # collision — coins
        for c in self.coins:
            if not c.collected and pr.colliderect(c.rect()):
                c.collected       = True
                self.coins_collected += 1
                self.score        += 50
                for _ in range(8):
                    self.particles.append(Particle(
                        c.x, c.y,
                        random.uniform(-3, 3), random.uniform(-5, 0),
                        18, 18, C_COIN, random.uniform(2, 5)
                    ))
        self.coins = [c for c in self.coins if not c.collected]

        for p in self.particles: p.update()
        self.particles = [p for p in self.particles if p.alive]

        if self.flash > 0:
            self.flash -= 1

    # ── Draw ──────────────────────────────────────────────────────────────────
    def draw(self):
        self.screen.fill(C_BG)
        self._draw_road()

        for c   in self.coins:     c.draw(self.screen, self.t)
        for obs in self.obstacles: obs.draw(self.screen)
        for p   in self.particles: p.draw(self.screen)

        if self.state == "playing":
            self.player.draw(self.screen)
        elif self.flash > 0:
            fl = pygame.Surface((W, H), pygame.SRCALPHA)
            fl.fill((255, 255, 255, int(180 * self.flash / 12)))
            self.screen.blit(fl, (0, 0))

        self._draw_hud()

        if self.state == "dead":
            self._draw_death()

        pygame.display.flip()

    # ── Main loop ─────────────────────────────────────────────────────────────
    def run(self):
        while True:
            self.clock.tick(FPS)
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        pygame.quit()
                        sys.exit()
                    if event.key == pygame.K_SPACE:
                        if self.state == "playing":
                            self.player.jump()
                        elif self.state == "dead":
                            self._reset()

            if self.state == "playing":
                self.update()
            else:
                self.t    += 1   # keep pulse animation running on death screen
                self.draw()      # keep drawing death screen
                continue

            self.draw()


# ─── Entry point ──────────────────────────────────────────────────────────────
def main():
    port = None

    if len(sys.argv) > 1:
        port = sys.argv[1]
        print(f"[serial] Using specified port: {port}")
    else:
        port = find_pico_port()
        if port:
            print(f"[serial] Auto-detected Pico on {port}")
        else:
            print("[serial] No Pico detected — running in keyboard-only mode.")

    game = Game()

    if port:
        t = threading.Thread(target=serial_thread, args=(game, port), daemon=True)
        t.start()

    game.run()


if __name__ == "__main__":
    main()