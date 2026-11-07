#!/usr/bin/env python3
"""Render the terminal-style demo art for 403-bypass into assets/ (dark + light).

Reuses the CentralTech/grimh3xx brand toolchain (JBM font embedding) from the
sibling `github-profile` repo so the artwork matches the rest of the profile.

Usage: python3 tools/gen_demo.py
"""
import sys
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets"

BRAND = Path.home() / "github-profile" / "scripts"
sys.path.insert(0, str(BRAND))
try:
    from brand import finish  # type: ignore
except Exception:  # pragma: no cover
    def finish(svg, faces=None):
        return svg

MONO = "'JBM','JetBrains Mono',ui-monospace,Consolas,Menlo,monospace"

THEMES = {
    "": {
        "bg": "#0B0F17", "bg2": "#0E1420", "bar": "#111826", "border": "#1E2A3D",
        "text": "#C5CFDD", "muted": "#8B98AD", "title": "#F1F5FB",
        "cyan": "#22D3EE", "blue": "#3B82F6",
        "red": "#F87171", "amber": "#FBBF24", "green": "#34D399",
    },
    "-light": {
        "bg": "#FFFFFF", "bg2": "#F6F8FB", "bar": "#EEF2F7", "border": "#D8DEE8",
        "text": "#334155", "muted": "#5B6678", "title": "#0F172A",
        "cyan": "#0891B2", "blue": "#2563EB",
        "red": "#DC2626", "amber": "#B45309", "green": "#059669",
    },
}

BANNER = r"""  _ _  ___ ____    _
 | | || _ \__ /   | |__ _  _ _ __  __ _ ___ ___
 |_  _||  _/|_ \   | '_ \ || | '_ \/ _` (_-<(_-<
   |_| |_| |___/   |_.__/\_, | .__/\__,_/__//__/
                        |__/|_|""".split("\n")


def seg(t, c=None, b=False):
    return (t, c, b)


def build_lines(th):
    lines = [
        [seg("$ ", "green", True),
         seg("403-bypass -u https://target.tld/admin -t 30 --only verb,path")],
        [],
    ]
    lines += [[seg(b, "cyan", True)] for b in BANNER]
    lines += [
        [seg("  403/401 bypass toolkit · authorized use only", "muted")],
        [],
        [seg("  alvo     https://target.tld/admin", "muted")],
        [seg("  baseline status="), seg("403", "red", True), seg("  0B")],
        [seg("  cod  tecnica   metodo   payload", "muted")],
        [seg("  403", "red", True), seg("  verb      GET      metodo GET")],
        [seg("  200", "green", True), seg("  verb      OPTIONS  metodo OPTIONS      "),
         seg("<== BYPASS?", "green", True)],
        [seg("  403", "red", True), seg("  header-ip GET      X-Forwarded-For: 127.0.0.1")],
        [seg("  200", "green", True), seg("  path      GET      /admin/..;/         "),
         seg("<== BYPASS?", "green", True)],
        [seg("  28 testes executados, "), seg("2 bypass(es) provavel(is)", "green", True)],
        [],
        [seg("$ ", "green", True), seg("make  &&  ./403-bypass -f urls.txt -o out.json")],
        [seg("  cc -O2 ... -o 403-bypass    "), seg("BUILD OK", "green", True),
         seg("  (0 warnings)", "muted")],
    ]
    return lines


def render(suffix):
    th = THEMES[suffix]
    lines = build_lines(th)
    pad_x, bar_h, top, lh, fs = 26, 40, 16, 21, 14.5
    ch = fs * 0.6
    w = 900
    h = bar_h + top + len(lines) * lh + top

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{int(h)}" '
        f'viewBox="0 0 {w} {int(h)}" role="img" aria-label="403-bypass demo">',
        '<defs>'
        f'<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">'
        f'<stop offset="0" stop-color="{th["bg"]}"/><stop offset="1" stop-color="{th["bg2"]}"/></linearGradient>'
        f'<clipPath id="r"><rect width="{w}" height="{int(h)}" rx="12"/></clipPath>'
        '</defs>',
        f'<g clip-path="url(#r)">',
        f'<rect width="{w}" height="{int(h)}" fill="url(#bg)"/>',
        f'<rect width="{w}" height="{bar_h}" fill="{th["bar"]}"/>',
        f'<circle cx="24" cy="{bar_h/2:.0f}" r="6" fill="#FF5F56"/>',
        f'<circle cx="44" cy="{bar_h/2:.0f}" r="6" fill="#FFBD2E"/>',
        f'<circle cx="64" cy="{bar_h/2:.0f}" r="6" fill="#27C93F"/>',
        f'<text x="{w/2:.0f}" y="{bar_h/2+5:.0f}" text-anchor="middle" font-family="{MONO}" '
        f'font-size="13" fill="{th["muted"]}">403-bypass — zsh</text>',
    ]

    y = bar_h + top + fs
    for line in lines:
        x = pad_x
        for text, color, bold in line:
            fill = th[color] if color else th["text"]
            weight = ' font-weight="700"' if bold else ""
            parts.append(
                f'<text x="{x:.1f}" y="{y:.1f}" font-family="{MONO}" font-size="{fs}" '
                f'fill="{fill}"{weight} xml:space="preserve">{escape(text)}</text>')
            x += len(text) * ch
        y += lh
    parts.append(f'</g><rect x="0.5" y="0.5" width="{w-1}" height="{int(h)-1}" rx="12" '
                 f'fill="none" stroke="{th["border"]}"/>')
    parts.append('</svg>\n')
    return finish("".join(parts), {("JBM", 400), ("JBM", 700)})


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    for suffix in THEMES:
        name = f"demo{suffix}.svg"
        (OUT / name).write_text(render(suffix), encoding="utf-8")
        print("wrote", OUT / name)
