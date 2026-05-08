#!/usr/bin/env python3
"""PintOS 프로젝트 문서용 SVG 다이어그램 6종 생성.

Toss-style design: white cards, subtle shadows, minimal color.
JetBrains font family, bezier curve arrows, polished spacing.
"""

from pathlib import Path
from xml.sax.saxutils import escape

# ---------------------------------------------------------------------------
# Design Tokens (Toss Style)
# ---------------------------------------------------------------------------
BG = "#FFFFFF"
CARD_BG = "#FFFFFF"
CARD_BORDER = "#F2F4F6"
TEXT_PRIMARY = "#191F28"
TEXT_BODY = "#333D4B"
TEXT_CAPTION = "#8B95A1"
DIVIDER = "#F2F4F6"
ACCENT_BLUE = "#3B82F6"
ARROW_COLOR = "#CBD5E1"

BADGE = {
    "blue":   {"bg": "#EFF6FF", "border": "#3B82F6", "text": "#1D4ED8"},
    "green":  {"bg": "#F0FDF4", "border": "#22C55E", "text": "#15803D"},
    "orange": {"bg": "#FFF7ED", "border": "#F97316", "text": "#C2410C"},
    "purple": {"bg": "#FAF5FF", "border": "#A855F7", "text": "#7E22CE"},
    "red":    {"bg": "#FEF2F2", "border": "#EF4444", "text": "#DC2626"},
}

PHASE_COLOR = {1: "blue", 2: "green", 3: "orange", 4: "purple"}

FONT_UI = "'Inter', 'JetBrains Sans', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif"
FONT_MONO = "'JetBrains Mono', 'SF Mono', 'Fira Code', 'Cascadia Code', monospace"

OUT_DIR = Path(__file__).resolve().parent.parent / "docs" / "pintos" / "img"


# ---------------------------------------------------------------------------
# Shared Defs (shadow filter + arrow marker)
# ---------------------------------------------------------------------------
def defs_block():
    return (
        '  <defs>\n'
        '    <filter id="shadow" x="-4%" y="-4%" width="108%" height="108%">\n'
        '      <feDropShadow dx="0" dy="2" stdDeviation="6" flood-color="#000" flood-opacity="0.06"/>\n'
        '    </filter>\n'
        f'    <marker id="arrow" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">\n'
        f'      <polygon points="0 0, 10 3.5, 0 7" fill="{ARROW_COLOR}"/>\n'
        '    </marker>\n'
        '  </defs>\n'
    )


def svg_open(w, h, title):
    return (
        f'<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
        f'viewBox="0 0 {w} {h}"\n'
        f'     font-family="{FONT_UI}">\n'
        f'  <style>\n'
        f'    .mono {{ font-family: {FONT_MONO}; }}\n'
        f'  </style>\n'
        f'  <rect width="{w}" height="{h}" fill="{BG}"/>\n'
        + defs_block()
        + f'  <text x="{w // 2}" y="56" text-anchor="middle" font-size="28" '
        f'font-weight="bold" fill="{TEXT_PRIMARY}">{escape(title)}</text>\n'
    )


def svg_close():
    return '</svg>\n'


# ---------------------------------------------------------------------------
# Components
# ---------------------------------------------------------------------------
def card(x, y, w, h):
    return (
        f'  <rect x="{x}" y="{y}" width="{w}" height="{h}" rx="16" '
        f'fill="{CARD_BG}" stroke="{CARD_BORDER}" stroke-width="1" filter="url(#shadow)"/>\n'
    )


def badge_el(x, y, label, color_key, w=None):
    """Render a pill badge. Auto-width if w is None."""
    c = BADGE[color_key]
    if w is None:
        w = len(label) * 8 + 24
    return (
        f'  <rect x="{x}" y="{y}" width="{w}" height="24" rx="12" fill="{c["bg"]}"/>\n'
        f'  <text x="{x + w // 2}" y="{y + 16}" font-size="12" fill="{c["text"]}" '
        f'text-anchor="middle" font-weight="bold">{escape(label)}</text>\n'
    )


def phase_badge(x, y, pn):
    return badge_el(x, y, f"Phase {pn}", PHASE_COLOR[pn])


def text_el(x, y, content, size=16, fill=TEXT_BODY, anchor="start", weight="normal", mono=False):
    cls = ' class="mono"' if mono else ""
    return (
        f'  <text x="{x}" y="{y}" font-size="{size}" fill="{fill}" '
        f'text-anchor="{anchor}" font-weight="{weight}"{cls}>{escape(content)}</text>\n'
    )


def arrow_line(x1, y1, x2, y2, with_arrow=True):
    """Arrow between two points. Short vertical = straight, long diagonal = curve."""
    end = ' marker-end="url(#arrow)"' if with_arrow else ""
    # Shorten endpoint by 8px
    dx = x2 - x1
    dy = y2 - y1
    dist = (dx * dx + dy * dy) ** 0.5
    if dist > 0:
        x2 = x2 - 8 * dx / dist
        y2 = y2 - 8 * dy / dist
    # Short vertical (under 100px) or nearly straight: use straight line
    if abs(dy) <= 100 or abs(dx) < 5:
        return (
            f'  <line x1="{x1:.0f}" y1="{y1:.0f}" x2="{x2:.0f}" y2="{y2:.0f}" '
            f'stroke="{ARROW_COLOR}" stroke-width="1.5"{end}/>\n'
        )
    # For mostly-horizontal arrows, use quadratic bezier with slight vertical offset
    if abs(dx) > abs(dy) * 1.5 and abs(dx) > 20:
        qx = (x1 + x2) / 2
        qy = min(y1, y2) - 10
        d = f"M {x1:.0f},{y1:.0f} Q {qx:.0f},{qy:.0f} {x2:.0f},{y2:.0f}"
        return (
            f'  <path d="{d}" fill="none" '
            f'stroke="{ARROW_COLOR}" stroke-width="1.5"{end}/>\n'
        )
    # For diagonal, use quadratic bezier
    qx = (x1 + x2) / 2
    qy = (y1 + y2) / 2 - 15
    d = f"M {x1:.0f},{y1:.0f} Q {qx:.0f},{qy:.0f} {x2:.0f},{y2:.0f}"
    return (
        f'  <path d="{d}" fill="none" '
        f'stroke="{ARROW_COLOR}" stroke-width="1.5"{end}/>\n'
    )


def path_el(d, with_arrow=True):
    end = ' marker-end="url(#arrow)"' if with_arrow else ""
    return (
        f'  <path d="{d}" fill="none" stroke="{ARROW_COLOR}" stroke-width="1.5"{end}/>\n'
    )


def divider(x1, y1, x2, y2):
    return (
        f'  <line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" '
        f'stroke="{DIVIDER}" stroke-width="1"/>\n'
    )


# ---------------------------------------------------------------------------
# 01 Architecture (1200x780)
# ---------------------------------------------------------------------------
def gen_01():
    W, H = 1200, 700
    s = svg_open(W, H, "Pintos Project 1 -- 모듈 구조")

    # Three cards side by side at y=100, with 60px gaps
    cw, ch = 300, 240
    gap = 60
    y0 = 100

    modules = [
        (80,           "timer.c",  1, ["timer_sleep()", "timer_interrupt()", "thread_awake()"]),
        (80+cw+gap,    "thread.c", 2, ["thread_create()", "thread_unblock()", "thread_yield()", "schedule()"]),
        (80+2*(cw+gap),"synch.c",  3, ["lock_acquire()", "lock_release()", "sema_down()", "sema_up()"]),
    ]

    for cx, name, pn, funcs in modules:
        s += card(cx, y0, cw, ch)
        # header
        s += text_el(cx + cw // 2, y0 + 40, name, 20, TEXT_PRIMARY, "middle", "bold")
        # badge
        s += phase_badge(cx + 30, y0 + 55, pn)
        # divider
        s += divider(cx + 20, y0 + 90, cx + cw - 20, y0 + 90)
        # function list (monospace)
        for j, fn in enumerate(funcs):
            s += text_el(cx + 30, y0 + 120 + j * 28, fn, 14, TEXT_BODY, mono=True)

    # Arrows between cards -- labels ABOVE arrows
    ay = y0 + ch // 2
    c1_right = 80 + cw
    c2_left = 80 + cw + gap
    c2_right = c2_left + cw
    c3_left = 80 + 2 * (cw + gap)
    mid1 = (c1_right + c2_left) // 2
    mid2 = (c2_right + c3_left) // 2

    s += arrow_line(c1_right, ay, c2_left, ay)
    s += text_el(mid1, ay - 14, "호출", 14, TEXT_CAPTION, "middle")

    s += arrow_line(c2_right, ay, c3_left, ay)
    s += text_el(mid2, ay - 14, "호출", 14, TEXT_CAPTION, "middle")

    # Full-width Phase 4 card
    p4y = 370
    s += card(80, p4y, 1040, 160)
    s += phase_badge(110, p4y + 20, 4)
    s += text_el(110 + 80 + 12, p4y + 36, "MLFQS", 20, TEXT_PRIMARY, "start", "bold")
    s += text_el(110, p4y + 74, "timer.c, thread.c, synch.c 세 모듈에 걸쳐 변경 사항 발생", 16, TEXT_BODY)
    s += text_el(110, p4y + 104, "nice, recent_cpu, load_avg 기반 우선순위 자동 계산 / priority donation 비활성화",
              14, TEXT_CAPTION)
    s += text_el(110, p4y + 132, "매 틱: recent_cpu++ / 매 초: load_avg, recent_cpu 재계산 / 매 4틱: priority 재계산",
              14, TEXT_CAPTION)

    # Legend at y=590
    ly = 590
    legend_items = [
        (1, "Phase 1"),
        (2, "Phase 2"),
        (3, "Phase 3"),
        (4, "Phase 4"),
    ]
    lx = 80
    for pn, label in legend_items:
        s += phase_badge(lx, ly, pn)
        lx += len(f"Phase {pn}") * 8 + 24 + 20

    s += svg_close()
    return s


# ---------------------------------------------------------------------------
# 02 Phase Dependency (1000x950)
# ---------------------------------------------------------------------------
def gen_02():
    W, H = 1000, 820
    s = svg_open(W, H, "Phase 의존 관계")

    cw, ch = 700, 110
    cx = 150

    spacing = ch + 60  # card height + gap
    phases = [
        (100,              1, "Alarm Clock", "timer_sleep 재구현, sleep_list 도입"),
        (100 + spacing,    2, "Priority Scheduling", "ready_list 우선순위 정렬, 선점 체크"),
        (100 + spacing*2,  3, "Priority Donation", "lock에서 우선순위 기부/회수"),
        (100 + spacing*3,  4, "MLFQS", "nice, recent_cpu, load_avg로 자동 계산"),
    ]

    for cy, pn, title, desc in phases:
        s += card(cx, cy, cw, ch)
        s += phase_badge(cx + 30, cy + 20, pn)
        bw = len(f"Phase {pn}") * 8 + 24
        s += text_el(cx + 30 + bw + 12, cy + 36, title, 20, TEXT_PRIMARY, "start", "bold")
        s += text_el(cx + 30, cy + 72, desc, 16, TEXT_BODY)

    # Arrows between cards -- vertical S-curves
    ax = 500
    for i, (cy, pn, _, _) in enumerate(phases[:-1]):
        next_cy = phases[i + 1][0]
        s += arrow_line(ax, cy + ch, ax, next_cy)

    s += svg_close()
    return s


# ---------------------------------------------------------------------------
# 03 Thread Lifecycle (1100x800)
# ---------------------------------------------------------------------------
def gen_03():
    W, H = 1100, 800
    s = svg_open(W, H, "스레드 생명 주기")

    sw, sh = 240, 80

    states = {
        "READY":   (400, 150),
        "BLOCKED": (80,  380),
        "RUNNING": (780, 380),
        "DYING":   (400, 620),
    }

    state_colors = {
        "READY":   BADGE["green"]["border"],
        "BLOCKED": BADGE["blue"]["border"],
        "RUNNING": BADGE["green"]["border"],
        "DYING":   TEXT_CAPTION,
    }

    for name, (x, y) in states.items():
        color = state_colors[name]
        s += (f'  <rect x="{x}" y="{y}" width="{sw}" height="{sh}" rx="16" '
              f'fill="{CARD_BG}" stroke="{color}" stroke-width="2" filter="url(#shadow)"/>\n')
        s += text_el(x + sw // 2, y + sh // 2 + 7, name, 20, TEXT_PRIMARY, "middle", "bold")

    # thread_create() -> READY (monospace label)
    rx, ry = states["READY"]
    s += text_el(rx + sw // 2, ry - 30, "thread_create()", 14, TEXT_CAPTION, "middle", mono=True)
    s += arrow_line(rx + sw // 2, ry - 20, rx + sw // 2, ry)

    # BLOCKED -> READY (bezier curve)
    bx, by = states["BLOCKED"]
    # Curve from BLOCKED top-right area to READY bottom-left area
    start_x = bx + sw
    start_y = by + 20
    end_x = rx + 8
    end_y = ry + sh - 8
    cx1 = (start_x + end_x) / 2 - 30
    cy1 = start_y - 60
    d = f"M {start_x},{start_y} Q {cx1:.0f},{cy1:.0f} {end_x},{end_y}"
    s += path_el(d)
    # Label offset from arrow
    label_x = (start_x + end_x) / 2 - 60
    label_y = cy1 - 8
    s += text_el(label_x, label_y, "thread_unblock()", 14, TEXT_CAPTION, "middle", mono=True)
    s += phase_badge(label_x - 50, label_y + 4, 1)

    # READY -> RUNNING (bezier curve)
    runx, runy = states["RUNNING"]
    start_x2 = rx + sw
    start_y2 = ry + sh - 20
    end_x2 = runx
    end_y2 = runy + 20
    cx2 = (start_x2 + end_x2) / 2 + 30
    cy2 = start_y2 + 40
    d2 = f"M {start_x2},{start_y2} Q {cx2:.0f},{cy2:.0f} {end_x2},{end_y2}"
    s += path_el(d2)
    label_x2 = cx2 + 20
    label_y2 = cy2 - 8
    s += text_el(label_x2, label_y2, "schedule()", 14, TEXT_CAPTION, "middle", mono=True)
    s += phase_badge(label_x2 - 50, label_y2 + 4, 2)

    # RUNNING -> BLOCKED (straight horizontal at same y, going left)
    start_x3 = runx
    start_y3 = runy + sh - 20
    end_x3 = bx + sw
    end_y3 = by + sh - 20
    d3 = f"M {start_x3},{start_y3} L {end_x3},{end_y3}"
    s += path_el(d3)
    # Label above the line
    mid_x3 = (start_x3 + end_x3) / 2
    mid_y3 = (start_y3 + end_y3) / 2
    s += text_el(mid_x3, mid_y3 - 22, "sema_down()", 14, TEXT_CAPTION, "middle", mono=True)
    s += phase_badge(mid_x3 - 40, mid_y3 - 16, 3)

    # RUNNING -> READY (wide arc ABOVE both cards)
    start_x4 = runx + sw // 2
    start_y4 = runy
    end_x4 = rx + sw - 20
    end_y4 = ry
    # Wide arc going high above
    arc_top = ry - 80
    cx4a = start_x4 + 40
    cy4a = arc_top - 20
    cx4b = end_x4 - 40
    cy4b = arc_top - 20
    d4 = f"M {start_x4},{start_y4} C {cx4a:.0f},{cy4a:.0f} {cx4b:.0f},{cy4b:.0f} {end_x4},{end_y4}"
    s += path_el(d4)
    # Label above the arc but with enough margin from top
    label_y4 = max(arc_top - 10, 80)
    s += text_el((start_x4 + end_x4) / 2, label_y4 - 20, "thread_yield()", 14, TEXT_CAPTION, "middle", mono=True)
    s += phase_badge((start_x4 + end_x4) / 2 - 50, label_y4 - 14, 2)

    # RUNNING -> DYING (curve down-left to DYING top)
    dx, dy = states["DYING"]
    start_x5 = runx + sw // 2
    start_y5 = runy + sh
    end_x5 = dx + sw // 2
    end_y5 = dy
    cx5 = start_x5
    cy5 = end_y5 - 30
    d5 = f"M {start_x5},{start_y5} Q {cx5},{cy5} {end_x5},{end_y5}"
    s += path_el(d5)
    label_x5 = (start_x5 + end_x5) / 2 + 40
    label_y5 = (start_y5 + end_y5) / 2
    s += text_el(label_x5, label_y5, "thread_exit()", 14, TEXT_CAPTION, "start", mono=True)

    s += svg_close()
    return s


# ---------------------------------------------------------------------------
# 04 Timer Interrupt (1100x900)
# ---------------------------------------------------------------------------
def gen_04():
    W, H = 1100, 900
    s = svg_open(W, H, "timer_interrupt() 처리 흐름")

    # Root card at top center
    rw, rh = 400, 60
    rx = (W - rw) // 2
    ry = 100
    s += card(rx, ry, rw, rh)
    s += text_el(rx + rw // 2, ry + 38, "timer_interrupt()", 20, TEXT_PRIMARY, "middle", "bold", mono=True)

    # Child cards
    cx = 80
    cw = 940

    # 1: ticks++
    c1y = 210
    c1h = 50
    s += card(cx, c1y, cw, c1h)
    s += text_el(cx + 30, c1y + 32, "ticks++", 16, TEXT_BODY, "start", "normal", mono=True)
    s += arrow_line(W // 2, ry + rh, W // 2, c1y)

    # 2: thread_tick()
    c2y = 300
    c2h = 50
    s += card(cx, c2y, cw, c2h)
    s += text_el(cx + 30, c2y + 32, "thread_tick() -- 타임 슬라이스 초과 시 선점", 16, TEXT_BODY)
    s += arrow_line(W // 2, c1y + c1h, W // 2, c2y)

    # 3: Phase 1 thread_awake
    c3y = 390
    c3h = 80
    s += card(cx, c3y, cw, c3h)
    s += phase_badge(cx + 20, c3y + 16, 1)
    bw = len("Phase 1") * 8 + 24
    s += text_el(cx + 20 + bw + 16, c3y + 32, "thread_awake() -- sleep 스레드 깨우기",
              16, TEXT_PRIMARY, "start", "bold")
    s += text_el(cx + 20 + bw + 16, c3y + 60,
              "sleep_list 순회, wake_tick <= ticks 인 스레드 unblock",
              14, TEXT_CAPTION)
    s += arrow_line(W // 2, c2y + c2h, W // 2, c3y)

    # 4: Phase 4 MLFQS large card
    c4y = 510
    c4h = 340
    s += card(cx, c4y, cw, c4h)
    s += phase_badge(cx + 20, c4y + 20, 4)
    bw4 = len("Phase 4") * 8 + 24
    s += text_el(cx + 20 + bw4 + 16, c4y + 36, "MLFQS 연산", 20, TEXT_PRIMARY, "start", "bold")
    s += arrow_line(W // 2, c3y + c3h, W // 2, c4y)

    # Sub-cards inside MLFQS
    sx = 110
    sw2 = 880

    # Sub 1: 매 틱
    s1y = 580
    s1h = 55
    s += card(sx, s1y, sw2, s1h)
    s += badge_el(sx + 20, s1y + 16, "매 틱", "purple")
    s += text_el(sx + 20 + 60 + 16, s1y + 34, "recent_cpu += 1 (현재 스레드)", 16, TEXT_BODY)

    # Sub 2: 매 초
    s2y = 660
    s2h = 75
    s += card(sx, s2y, sw2, s2h)
    s += badge_el(sx + 20, s2y + 16, "매 초", "purple")
    s += text_el(sx + 20 + 56 + 16, s2y + 34, "load_avg 재계산", 16, TEXT_BODY)
    s += text_el(sx + 20 + 56 + 16, s2y + 62, "recent_cpu 전체 스레드 재계산", 16, TEXT_BODY)

    # Sub 3: 매 4틱
    s3y = 760
    s3h = 55
    s += card(sx, s3y, sw2, s3h)
    s += badge_el(sx + 20, s3y + 16, "매 4틱", "purple")
    s += text_el(sx + 20 + 72 + 16, s3y + 34, "priority 재계산 (전체 스레드)", 16, TEXT_BODY)

    s += svg_close()
    return s


# ---------------------------------------------------------------------------
# 05 Priority Donation (1200x830)
# ---------------------------------------------------------------------------
def gen_05():
    W, H = 1200, 720
    s = svg_open(W, H, "Priority Donation 시나리오")

    # Horizontal timeline at y=250
    tl_y = 250
    s += (f'  <line x1="100" y1="{tl_y}" x2="1100" y2="{tl_y}" '
          f'stroke="{TEXT_CAPTION}" stroke-width="2"/>\n')

    events = [
        (200,  "L(pri=1)이\nLock A 획득",    "pri=1"),
        (450,  "H(pri=63)이\nLock A 요청",   "63 기부"),
        (700,  "L(pri=63) 완료\nLock A 해제", "pri=1 복원"),
        (950,  "H(pri=63)\nLock A 획득",     "pri=63"),
    ]

    ecw, ech = 180, 70
    for ex, lines_raw, badge_text in events:
        # Dot on timeline -- larger with white stroke ring
        s += f'  <circle cx="{ex}" cy="{tl_y}" r="6" fill="{BADGE["orange"]["border"]}" stroke="{BG}" stroke-width="2"/>\n'

        # Card above
        ecy = tl_y - 95
        ecx = ex - ecw // 2
        s += card(ecx, ecy, ecw, ech)
        parts = lines_raw.split("\n")
        s += text_el(ex, ecy + 30, parts[0], 14, TEXT_PRIMARY, "middle")
        if len(parts) > 1:
            s += text_el(ex, ecy + 50, parts[1], 14, TEXT_PRIMARY, "middle")

        # Connector
        s += (f'  <line x1="{ex}" y1="{ecy + ech}" x2="{ex}" y2="{tl_y - 8}" '
              f'stroke="{CARD_BORDER}" stroke-width="1"/>\n')

        # Badge below
        s += badge_el(ex - 50, tl_y + 24, badge_text, "orange", 100)

    # Comparison cards below
    comp_x = 80
    comp_w = 1040

    # Without donation (red left border)
    wy = 380
    wh = 130
    s += card(comp_x, wy, comp_w, wh)
    s += (f'  <rect x="{comp_x}" y="{wy + 16}" width="4" height="{wh - 32}" rx="2" '
          f'fill="{BADGE["red"]["border"]}"/>\n')
    s += text_el(comp_x + 28, wy + 32, "기부 없을 때 (우선순위 역전 발생)", 16, TEXT_PRIMARY, "start", "bold")

    # Three bars with more horizontal padding
    bar_y = wy + 56
    bar_h = 28
    bar_start = comp_x + 32
    # L running long
    s += (f'  <rect x="{bar_start}" y="{bar_y}" width="300" height="{bar_h}" rx="6" '
          f'fill="{BADGE["red"]["bg"]}" stroke="{BADGE["red"]["border"]}" stroke-width="1"/>\n')
    s += text_el(bar_start + 150, bar_y + 19, "L(pri=1) Lock A 보유", 12, BADGE["red"]["text"], "middle", "bold")
    # M interrupts
    s += (f'  <rect x="{bar_start + 316}" y="{bar_y}" width="200" height="{bar_h}" rx="6" '
          f'fill="{BADGE["orange"]["bg"]}" stroke="{BADGE["orange"]["border"]}" stroke-width="1"/>\n')
    s += text_el(bar_start + 416, bar_y + 19, "M(pri=31) 선점", 12, BADGE["orange"]["text"], "middle", "bold")
    # H waiting
    s += (f'  <rect x="{bar_start + 532}" y="{bar_y}" width="300" height="{bar_h}" rx="6" '
          f'fill="{BADGE["blue"]["bg"]}" stroke="{BADGE["blue"]["border"]}" stroke-width="1"/>\n')
    s += text_el(bar_start + 682, bar_y + 19, "H(pri=63) 대기 -- 역전!", 12, BADGE["blue"]["text"], "middle", "bold")
    s += text_el(bar_start + 856, bar_y + 19, "H가 L보다 늦게 실행됨", 14, BADGE["red"]["text"])

    # With donation (green left border)
    gy = 540
    gh = 130
    s += card(comp_x, gy, comp_w, gh)
    s += (f'  <rect x="{comp_x}" y="{gy + 16}" width="4" height="{gh - 32}" rx="2" '
          f'fill="{BADGE["green"]["border"]}"/>\n')
    s += text_el(comp_x + 28, gy + 32, "기부 있을 때 (해결)", 16, TEXT_PRIMARY, "start", "bold")

    bar_y2 = gy + 56
    # L fast
    s += (f'  <rect x="{bar_start}" y="{bar_y2}" width="200" height="{bar_h}" rx="6" '
          f'fill="{BADGE["green"]["bg"]}" stroke="{BADGE["green"]["border"]}" stroke-width="1"/>\n')
    s += text_el(bar_start + 100, bar_y2 + 19, "L(pri=63) 기부받아 실행", 12, BADGE["green"]["text"], "middle", "bold")
    # H runs next
    s += (f'  <rect x="{bar_start + 216}" y="{bar_y2}" width="300" height="{bar_h}" rx="6" '
          f'fill="{BADGE["blue"]["bg"]}" stroke="{BADGE["blue"]["border"]}" stroke-width="1"/>\n')
    s += text_el(bar_start + 366, bar_y2 + 19, "H(pri=63) 바로 실행", 12, BADGE["blue"]["text"], "middle", "bold")
    # M last
    s += (f'  <rect x="{bar_start + 532}" y="{bar_y2}" width="200" height="{bar_h}" rx="6" '
          f'fill="{BADGE["orange"]["bg"]}" stroke="{BADGE["orange"]["border"]}" stroke-width="1"/>\n')
    s += text_el(bar_start + 632, bar_y2 + 19, "M(pri=31) 마지막", 12, BADGE["orange"]["text"], "middle", "bold")
    s += text_el(bar_start + 756, bar_y2 + 19, "H가 빠르게 실행됨", 14, BADGE["green"]["text"])

    s += svg_close()
    return s


# ---------------------------------------------------------------------------
# 06 Struct Thread (1000x980)
# ---------------------------------------------------------------------------
def gen_06():
    W, H = 1000, 1120
    s = svg_open(W, H, "struct thread 필드 구성")

    cx = 100
    cw = 800
    row_h = 36
    header_h = 68  # more gap from header to column headers
    col_header_gap = 16  # gap between column headers and first data row
    y = 90

    col_type_x = cx + 30
    col_field_x = cx + 260
    col_desc_x = cx + 460

    groups = [
        (None, "기본 필드 (Original)", [
            ("tid_t",              "tid",      "스레드 식별자"),
            ("enum thread_status", "status",   "현재 상태"),
            ("char[16]",           "name",     "스레드 이름"),
            ("int",                "priority", "우선순위 (0-63)"),
            ("struct list_elem",   "elem",     "리스트 요소"),
            ("struct intr_frame",  "tf",       "인터럽트 프레임"),
            ("unsigned",           "magic",    "스택 오버플로 감지"),
        ]),
        (1, "추가 필드", [
            ("int64_t", "wake_tick", "깨어날 틱"),
        ]),
        (3, "추가 필드", [
            ("int",              "original_priority", "기부 전 원래 우선순위"),
            ("struct lock *",    "wait_on_lock",      "대기 중인 락"),
            ("struct list",      "donations",         "기부 목록"),
            ("struct list_elem", "donation_elem",     "기부 리스트 요소"),
        ]),
        (4, "추가 필드", [
            ("int", "nice",       "nice 값 (-20~20)"),
            ("int", "recent_cpu", "최근 CPU 사용량 (고정소수점)"),
        ]),
    ]

    for pn, group_title, fields in groups:
        card_h = header_h + col_header_gap + len(fields) * row_h + 28
        s += card(cx, y, cw, card_h)

        # Header row (20px gap from card header to content)
        if pn is not None:
            s += phase_badge(cx + 30, y + 20, pn)
            bw = len(f"Phase {pn}") * 8 + 24
            s += text_el(cx + 30 + bw + 12, y + 36, group_title, 20, TEXT_PRIMARY, "start", "bold")
        else:
            s += text_el(cx + 30, y + 36, group_title, 20, TEXT_PRIMARY, "start", "bold")

        # Column headers (16px gap before first data row)
        chy = y + header_h
        s += text_el(col_type_x, chy, "Type", 14, TEXT_CAPTION, "start", "bold")
        s += text_el(col_field_x, chy, "Field", 14, TEXT_CAPTION, "start", "bold")
        s += text_el(col_desc_x, chy, "Description", 14, TEXT_CAPTION, "start", "bold")
        s += divider(cx + 20, chy + 8, cx + cw - 20, chy + 8)

        # Rows -- 36px per row, type column in monospace
        first_row_y = chy + 8 + col_header_gap
        for i, (ftype, fname, fdesc) in enumerate(fields):
            fy = first_row_y + i * row_h + row_h // 2 + 5
            if i > 0:
                s += divider(cx + 20, first_row_y + i * row_h, cx + cw - 20, first_row_y + i * row_h)
            s += text_el(col_type_x, fy, ftype, 14, TEXT_BODY, mono=True)
            s += text_el(col_field_x, fy, fname, 16, TEXT_PRIMARY, "start", "bold")
            s += text_el(col_desc_x, fy, fdesc, 14, TEXT_CAPTION)

        y += card_h + 20

    # Ensure we have at least 40px clearance at bottom
    # Adjust H if needed (handled by fixed H = 980 + 20 extra)
    s += svg_close()
    return s


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    diagrams = [
        ("01-architecture.svg",      gen_01),
        ("02-phase-dependency.svg",  gen_02),
        ("03-thread-lifecycle.svg",  gen_03),
        ("04-timer-interrupt.svg",   gen_04),
        ("05-priority-donation.svg", gen_05),
        ("06-struct-thread.svg",     gen_06),
    ]

    for filename, gen_func in diagrams:
        path = OUT_DIR / filename
        svg_content = gen_func()
        path.write_text(svg_content, encoding="utf-8")
        print(f"[OK] {path}")

    print(f"\nAll {len(diagrams)} diagrams generated in {OUT_DIR}")


if __name__ == "__main__":
    main()
