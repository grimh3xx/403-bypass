#!/usr/bin/env python3
"""Render the README banner + social preview for 403-bypass into docs/.

Same model as the rest of the grimh3xx profile (CentralTech brand): reuses
brand.py (palette + JBM/Inter font embedding) from the sibling github-profile
repo. Mirrors scripts/repo_art.py:readme_banner / social.

Usage: python3 tools/gen_banner.py
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs"

BRAND = Path.home() / "github-profile" / "scripts"
sys.path.insert(0, str(BRAND))
from brand import DARK, LANG_COLORS, MONO, SANS, escape, finish  # type: ignore

TH = DARK
NAME = "403-bypass"
TAGLINE = "403/401 bypass toolkit in C"
TAGS = ["C", "libcurl", "HTTP"]


def chips(items, x, y, fs):
    out = []
    for it in items:
        dot = LANG_COLORS.get(it)
        w = len(it) * fs * 0.6 + fs * 2 + (fs * 1.1 if dot else 0)
        h = fs * 2
        out.append(f'<rect x="{x:.0f}" y="{y:.0f}" width="{w:.0f}" height="{h:.0f}" '
                   f'rx="{h / 2:.0f}" fill="{TH["accent"]}" fill-opacity="0.08" '
                   f'stroke="{TH["border"]}"/>')
        tx = x + fs
        if dot:
            out.append(f'<circle cx="{tx + fs * 0.3:.0f}" cy="{y + h / 2:.0f}" '
                       f'r="{fs * 0.3:.1f}" fill="{dot}"/>')
            tx += fs * 1.1
        out.append(f'<text x="{tx:.0f}" y="{y + h / 2 + fs * 0.36:.0f}" '
                   f'font-family="{MONO}" font-size="{fs}" fill="{TH["text"]}">{escape(it)}</text>')
        x += w + fs * 0.7
    return "".join(out)


def basesvg(w, h, inner):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">
<defs>
<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="{TH["bg"]}"/><stop offset="1" stop-color="{TH["bg2"]}"/></linearGradient>
<linearGradient id="brand" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="{TH["accent"]}"/><stop offset="1" stop-color="{TH["accent2"]}"/></linearGradient>
<pattern id="dots" width="24" height="24" patternUnits="userSpaceOnUse"><circle cx="1" cy="1" r="1" fill="{TH["grid"]}" fill-opacity="0.16"/></pattern>
<radialGradient id="glow" cx="0.2" cy="0.1" r="0.8"><stop offset="0" stop-color="{TH["accent"]}" stop-opacity="0.2"/><stop offset="1" stop-color="{TH["accent"]}" stop-opacity="0"/></radialGradient>
<filter id="soft" x="-10%" y="-50%" width="120%" height="200%"><feGaussianBlur stdDeviation="12"/></filter>
<clipPath id="r"><rect width="{w}" height="{h}" rx="16"/></clipPath>
</defs>
<g clip-path="url(#r)">
<rect width="{w}" height="{h}" fill="url(#bg)"/><rect width="{w}" height="{h}" fill="url(#dots)"/><rect width="{w}" height="{h}" fill="url(#glow)"/>
{inner}
</g>
<rect x="0.5" y="0.5" width="{w - 1}" height="{h - 1}" rx="16" fill="none" stroke="{TH["border"]}"/>
</svg>
'''


def wordmark(x, y, name, size):
    t = (f'<text x="{x}" y="{y}" font-family="{MONO}" font-size="{size}" '
         f'font-weight="800" letter-spacing="-1">{escape(name)}</text>')
    return (f'<g fill="{TH["accent"]}" opacity="0.35" filter="url(#soft)">{t}</g>'
            f'<g fill="url(#brand)">{t}</g>')


def readme_banner():
    w, h = 1000, 220
    inner = (
        f'<text x="48" y="58" font-family="{MONO}" font-size="12" letter-spacing="3" '
        f'fill="{TH["muted"]}"><tspan fill="{TH["accent2"]}">●</tspan>  CENTRALTECH  ·  GRIMH3XX</text>'
        + wordmark(46, 122, NAME, 54)
        + f'<text x="48" y="158" font-family="{SANS}" font-size="18" fill="{TH["text"]}">{escape(TAGLINE)}</text>'
        + chips(TAGS, 48, 176, 12)
    )
    return finish(basesvg(w, h, inner), {("JBM", 400), ("JBM", 800), ("INT", 400)})


def social():
    w, h = 1280, 640
    size = min(110, int(2000 / max(len(NAME), 8)))
    inner = (
        f'<text x="96" y="150" font-family="{MONO}" font-size="20" letter-spacing="4" '
        f'fill="{TH["muted"]}"><tspan fill="{TH["accent2"]}">●</tspan>  CENTRALTECH  ·  GRIMH3XX</text>'
        + wordmark(90, 300, NAME, size)
        + f'<text x="96" y="376" font-family="{SANS}" font-size="36" fill="{TH["text"]}">{escape(TAGLINE)}</text>'
        + chips(TAGS, 96, 420, 22)
        + f'<path d="M96 540H{w - 96}" stroke="{TH["border"]}"/>'
        + f'<text x="96" y="584" font-family="{MONO}" font-size="22" fill="{TH["muted"]}">github.com/grimh3xx</text>'
        + f'<text x="{w - 96}" y="584" text-anchor="end" font-family="{MONO}" font-size="22" fill="{TH["muted"]}">centraltechpc.com</text>'
    )
    return finish(basesvg(w, h, inner), {("JBM", 400), ("JBM", 800), ("INT", 400)})


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    (OUT / "banner.svg").write_text(readme_banner(), encoding="utf-8")
    (OUT / "social-preview.svg").write_text(social(), encoding="utf-8")
    print("wrote", OUT / "banner.svg", "and", OUT / "social-preview.svg")
