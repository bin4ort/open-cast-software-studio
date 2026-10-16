#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Open Cast Software Studio contributors
"""
OCS Studio logo generator.

All variants share one square (1:1, 512x512) canvas with a single vivid
waveform:

  main - a "processed" chirp: frequency rises left to right along a mild
         S-curve, opening with a small crest towards the top, swelling to
         full amplitude with per-cycle dynamics, a soft "breath" pause
         around two thirds in, and a raised-cosine fade. Its stroke tapers
         from thick to thin.

  shadow - the colored line is drawn with a slight drop shadow (an offset
           translucent copy, no SVG filters, so it renders in Qt too).

Variants:

  logo.svg                 primary mark (gradient + shadow)
  logo-light.svg           light-theme mark (dark ink, for white backgrounds)
  logo-dark.svg            dark-theme mark (pale, for black backgrounds)
  logo-framed.svg          primary mark with a round frame
  logo-framed-light.svg    light-theme mark with a round frame
  logo-framed-dark.svg     dark-theme mark with a round frame
  logo-colorful.svg        multicolored main wave (one hue per cycle)
  logo-mono.svg            monochrome single-color mark (favicons, print)
  logo-adaptive.svg        theme-adaptive via CSS prefers-color-scheme (web)
  logo-tile.svg            filled tile design (mobile/launcher use)

Run:  python3 assets/generate_logo.py
"""

import math
import os

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
S = 512  # square canvas

# ---------------------------------------------------------------------------
# shared geometry
# ---------------------------------------------------------------------------

def smoothstep(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3.0 - 2.0 * x)


def lead_amp(t):
    """A proper small crest at the start (round, towards the top), then the
    phrase swells to full amplitude."""
    return 0.42 + 0.58 * smoothstep((t - 0.40) / 0.10)


def envelope(t):
    """Voice-like amplitude envelope: the line starts at full amplitude with
    the wide first cycle (like the original mark), then fades with a
    raised-cosine curve to 35% over the last 14%. Zero slope at the fade
    joints - no linear ramps."""
    fade = 1.0 - 0.65 * 0.5 * (
        1.0 - math.cos(math.pi * min(1.0, max(0.0, (t - 0.86) / 0.14)))
    )
    return fade


def dynamics(t):
    """Per-cycle loudness variation, like a real phrase: an uneven swell,
    phase-aligned so the big crests land on the swells. Blends in after the
    small lead-in crest so the start stays clean and round."""
    d = (0.72 + 0.18 * math.sin(2.0 * math.pi * 2.2 * t - 5.76)
         + 0.06 * math.sin(2.0 * math.pi * 5.1 * t + 0.8))
    return 1.0 + (d - 1.0) * smoothstep((t - 0.45) / 0.20)


def breath(t):
    """A soft pause between the crests (a breath between phrases)."""
    return 1.0 - 0.6 * math.exp(-((t - 0.76) / 0.05) ** 2)


def chirp_phase(t, f0, f1):
    """Phase of a frequency-rising chirp whose frequency follows a mild
    S-curve (gentle at both ends, no linear ramp, no density spike in the
    middle). Analytic via the integral of 0.5 + 0.5*sin(pi*(2u-1)/2)."""
    s_int = 0.5 * t - (1.0 / (2.0 * math.pi)) * math.cos(
        math.pi * (2.0 * t - 1.0) / 2.0)
    return 2.0 * math.pi * (f0 * t + (f1 - f0) * s_int)


# The first half-cycle is a custom shape instead of a sine: a smooth arc
# down to the low peak (moved right, halfway to the crest), then a clean
# smoothstep flank up to the small crest - no kinks, smooth angles at both
# ends, and a straighter line between the peaks than the old bump + sine.
T_CREST = 0.31

def first_cycle_offset(t):
    """Offset from the center line for the first half-cycle (positive =
    below). Ends exactly at the small crest (+63) with zero slope, where the
    sine phrase takes over seamlessly."""
    u = t / T_CREST
    low_at = 0.5          # the low peak sits halfway to the crest
    low_depth = 74.0
    crest_h = 63.0
    if u <= low_at:
        return low_depth * 0.5 * (1.0 - math.cos(math.pi * u / low_at))
    z = (u - low_at) / (1.0 - low_at)
    return low_depth - (crest_h + low_depth) * smoothstep(z)


def squeeze(t):
    """Pulls the later features towards the center: after the small crest
    the phase advances a little faster, so the largest trough and crest land
    closer to the middle of the canvas (asymmetric symmetry)."""
    if t < T_CREST:
        return 0.0
    return 1.55 * smoothstep((t - T_CREST) / 0.12)


def wave_pts(amp, y0, delay, jitter, drift, f0=0.45, f1=5.0, n=360):
    """One waveform. `delay` lags the amplitude dynamics, `jitter` adds
    irregular rhythm and `drift` pulls the phase out of sync."""
    pts = []
    for i in range(n + 1):
        t = i / n
        if t < T_CREST:
            v = (amp / 150.0) * first_cycle_offset(t)
            pts.append((64.0 + 384.0 * t, y0 + v))
            continue
        td = max(0.0, t - delay * 0.10)
        a = amp * envelope(td) * lead_amp(td) * dynamics(td) * breath(td)
        fail = drift * (t - T_CREST) * (t - T_CREST)
        ph = (chirp_phase(t, f0, f1) + squeeze(t) + fail
              + jitter * math.sin(2.0 * math.pi * 1.7 * t + 0.8))
        pts.append((64.0 + 384.0 * t, y0 - a * math.sin(ph)))
    return pts


MAIN = wave_pts(150.0, 256.0, 0.0, 0.0, 0.0)


def curvature_radius(pts, i, n):
    """Local radius of curvature of the polyline at sample i (px)."""
    i0, i2 = max(0, i - 1), min(n, i + 1)
    x0, y0 = pts[i0]
    x1, y1 = pts[i]
    x2, y2 = pts[i2]
    dx = (x2 - x0) / 2.0
    dy = (y2 - y0) / 2.0
    d2x = x0 - 2.0 * x1 + x2
    d2y = y0 - 2.0 * y1 + y2
    denom = abs(dx * d2y - dy * d2x)
    if denom < 1e-9:
        return 1e9
    return (dx * dx + dy * dy) ** 1.5 / denom


def path_of(pts):
    d = f"M {pts[0][0]:.1f} {pts[0][1]:.1f}"
    for x, y in pts[1:]:
        d += f" L {x:.1f} {y:.1f}"
    return d


def tapered(pts, w_start, w_end, nseg=16):
    """Splits a wave into overlapping round-capped segments whose stroke
    width tapers from w_start to w_end - a smooth variable-width line."""
    out = []
    chunk = (len(pts) - 1) / nseg
    for k in range(nseg):
        i0 = int(round(k * chunk))
        i1 = int(round((k + 1) * chunk))
        w = w_start + (w_end - w_start) * (k + 0.5) / nseg
        out.append((path_of(pts[i0:i1 + 1]), w))
    return out


MAIN_SEG = tapered(MAIN, 22.0, 10.0, nseg=10)


def header(extra=""):
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        "<!-- SPDX-License-Identifier: GPL-3.0-only -->\n"
        "<!-- Copyright (C) 2026 Open Cast Software Studio contributors -->\n"
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {S} {S}" '
        f'width="{S}" height="{S}">\n{extra}'
    )

FOOTER = "</svg>\n"

GRADIENT = {
    "primary": ('    <linearGradient id="wave" gradientUnits="userSpaceOnUse" '
                'x1="64" y1="0" x2="448" y2="0">\n'
                '      <stop offset="0" stop-color="#22d3ee"/>\n'
                '      <stop offset="0.55" stop-color="#60a5fa"/>\n'
                '      <stop offset="1" stop-color="#a78bfa"/>\n'
                "    </linearGradient>\n"),
    "light": ('    <linearGradient id="wave" gradientUnits="userSpaceOnUse" '
              'x1="64" y1="0" x2="448" y2="0">\n'
              '      <stop offset="0" stop-color="#0ea5e9"/>\n'
              '      <stop offset="0.55" stop-color="#4f46e5"/>\n'
              '      <stop offset="1" stop-color="#7c3aed"/>\n'
              "    </linearGradient>\n"),
    "dark": ('    <linearGradient id="wave" gradientUnits="userSpaceOnUse" '
             'x1="64" y1="0" x2="448" y2="0">\n'
             '      <stop offset="0" stop-color="#67e8f9"/>\n'
             '      <stop offset="0.55" stop-color="#93c5fd"/>\n'
             '      <stop offset="1" stop-color="#c4b5fd"/>\n'
             "    </linearGradient>\n"),
}


def frame_tinted(stroke, opacity):
    return (f'  <circle cx="{S / 2}" cy="{S / 2}" r="232" fill="none"\n'
            f'          stroke="{stroke}" stroke-opacity="{opacity}" '
            f'stroke-width="12"/>\n')


def segments_block(segments, stroke, opacity=1.0, grad=None):
    """Emits the tapered segments of one wave. `grad` (gradient id) colors
    the stroke, otherwise `stroke` is used."""
    out = []
    for d, w in segments:
        color = f"url(#{grad})" if grad else stroke
        out.append(f'  <path d="{d}" fill="none" stroke="{color}"\n'
                   f'        stroke-width="{w:.1f}" stroke-opacity="{opacity}"\n'
                   '        stroke-linecap="round" stroke-linejoin="round"/>')
    return "\n".join(out) + "\n"


SHADOW_COLOR = {"primary": "#0b1424", "light": "#1e293b", "dark": "#000000"}
SHADOW_OPACITY = {"primary": 0.22, "light": 0.25, "dark": 0.35}


def shadow_block(segments, color, opacity, dx=3.0, dy=4.0):
    """A slight drop shadow: the same segments, offset and translucent.
    Drawn as a plain offset copy (no SVG filters) so it renders everywhere,
    including the Qt icon renderer."""
    out = []
    for d, w in segments:
        out.append(f'  <path d="{d}" fill="none" stroke="{color}"\n'
                   f'        stroke-width="{w:.1f}" stroke-opacity="{opacity}"\n'
                   f'        transform="translate({dx} {dy})"\n'
                   '        stroke-linecap="round" stroke-linejoin="round"/>')
    return "\n".join(out) + "\n"


def wave_group(grad_key):
    """The colored wave with a slight drop shadow."""
    return (shadow_block(MAIN_SEG, SHADOW_COLOR[grad_key],
                         SHADOW_OPACITY[grad_key]) +
            segments_block(MAIN_SEG, None, grad="wave"))


def write(name, body):
    with open(os.path.join(OUT_DIR, name), "w") as f:
        f.write(body)
    print(f"wrote {name}")


def logo_primary():
    write("logo.svg", header("  <defs>\n" + GRADIENT["primary"] + "  </defs>\n")
          + wave_group("primary") + FOOTER)


def logo_light():
    write("logo-light.svg", header("  <defs>\n" + GRADIENT["light"] + "  </defs>\n")
          + wave_group("light") + FOOTER)


def logo_dark():
    write("logo-dark.svg", header("  <defs>\n" + GRADIENT["dark"] + "  </defs>\n")
          + wave_group("dark") + FOOTER)


def logo_framed():
    write("logo-framed.svg", header("  <defs>\n" + GRADIENT["primary"] + "  </defs>\n")
          + frame_tinted("#7c8ba3", 0.55) + wave_group("primary") + FOOTER)


def logo_framed_light():
    write("logo-framed-light.svg", header("  <defs>\n" + GRADIENT["light"] + "  </defs>\n")
          + frame_tinted("#334155", 0.85) + wave_group("light") + FOOTER)


def logo_framed_dark():
    write("logo-framed-dark.svg", header("  <defs>\n" + GRADIENT["dark"] + "  </defs>\n")
          + frame_tinted("#e6ebf5", 0.5) + wave_group("dark") + FOOTER)


def logo_mono():
    write("logo-mono.svg", header()
          + shadow_block(MAIN_SEG, "#0b1424", 0.22)
          + segments_block(MAIN_SEG, "#334155") + FOOTER)


def logo_colorful():
    colors = ["#22d3ee", "#38bdf8", "#818cf8", "#c084fc", "#f472b6", "#fb7185"]
    f0, f1 = 0.45, 6.5
    n = 420
    dense = []
    for i in range(n + 1):
        t = i / n
        if t < T_CREST:
            dense.append((64.0 + 384.0 * t, 256.0 + first_cycle_offset(t)))
            continue
        td = t
        a = 150.0 * envelope(td) * lead_amp(td) * dynamics(td) * breath(td)
        dense.append((64.0 + 384.0 * t,
                      256.0 - a * math.sin(chirp_phase(t, f0, f1))))
    segments = [[]]
    for i, (x, y) in enumerate(dense):
        t = i / n
        cyc = int(chirp_phase(t, f0, f1) / (2.0 * math.pi))
        if cyc >= len(segments):
            segments.append([])
        segments[cyc].append((x, y))
    body = header()
    body += f'  <path d="{path_of(dense)}" fill="none" stroke="#0b1424"\n' \
            '        stroke-width="16" stroke-opacity="0.22"\n' \
            '        transform="translate(3 4)" stroke-linecap="round" ' \
            'stroke-linejoin="round"/>\n'
    # color per cycle, tapering width per cycle
    for idx, seg in enumerate(segments):
        if not seg:
            continue
        for d, w in tapered(seg, 22.0, 10.0, nseg=1):
            body += f'  <path d="{d}" fill="none" ' \
                    f'stroke="{colors[idx % len(colors)]}" stroke-width="{w:.1f}"\n' \
                    '        stroke-linecap="round" stroke-linejoin="round"/>\n'
    body += FOOTER
    write("logo-colorful.svg", body)


def logo_adaptive():
    def block(grad, shadow_color):
        return (shadow_block(MAIN_SEG, shadow_color, 0.22) +
                segments_block(MAIN_SEG, None, grad=grad))
    write("logo-adaptive.svg", header(
        "  <style>\n"
        "    .dark-wave { display: none; }\n"
        "    @media (prefers-color-scheme: dark) {\n"
        "      .light-wave { display: none; }\n"
        "      .dark-wave  { display: inline; }\n"
        "    }\n"
        "  </style>\n") +
        f'  <g class="light-wave">\n{block("wLight", "#0b1424")}\n  </g>\n'
        f'  <g class="dark-wave">\n{block("wDark", "#000000")}\n  </g>\n'
        "  <defs>\n"
        '    <linearGradient id="wLight" gradientUnits="userSpaceOnUse" '
        'x1="64" y1="0" x2="448" y2="0">\n'
        '      <stop offset="0" stop-color="#0ea5e9"/>\n'
        '      <stop offset="0.55" stop-color="#4f46e5"/>\n'
        '      <stop offset="1" stop-color="#7c3aed"/>\n'
        "    </linearGradient>\n"
        '    <linearGradient id="wDark" gradientUnits="userSpaceOnUse" '
        'x1="64" y1="0" x2="448" y2="0">\n'
        '      <stop offset="0" stop-color="#67e8f9"/>\n'
        '      <stop offset="0.55" stop-color="#93c5fd"/>\n'
        '      <stop offset="1" stop-color="#c4b5fd"/>\n'
        "    </linearGradient>\n"
        "  </defs>\n" + FOOTER)


def logo_tile():
    write("logo-tile.svg",
          header() +
          "  <defs>\n"
          '    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">\n'
          '      <stop offset="0" stop-color="#101426"/>\n'
          '      <stop offset="1" stop-color="#1d2440"/>\n'
          "    </linearGradient>\n" +
          GRADIENT["primary"] +
          "  </defs>\n" +
          '  <rect x="16" y="16" width="480" height="480" rx="112" '
          'fill="url(#bg)"/>\n'
          '  <rect x="16" y="16" width="480" height="480" rx="112" fill="none"\n'
          '        stroke="#ffffff" stroke-opacity="0.08" stroke-width="3"/>\n'
          '  <circle cx="256" cy="256" r="178" fill="none" stroke="#ffffff"\n'
          '          stroke-opacity="0.05" stroke-width="26"/>\n'
          + shadow_block(MAIN_SEG, "#000000", 0.3)
          + segments_block(MAIN_SEG, None, grad="wave")
          + FOOTER)


if __name__ == "__main__":
    logo_primary()
    logo_light()
    logo_dark()
    logo_framed()
    logo_framed_light()
    logo_framed_dark()
    logo_mono()
    logo_colorful()
    logo_adaptive()
    logo_tile()
    print("done")
